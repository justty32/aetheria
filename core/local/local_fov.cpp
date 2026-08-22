// local_fov.cpp：以整數 DDA 對每個候選格投射穿邊光線，牆與非開啟門遮蔽。

#include "core/local/local_fov.h"

#include <aetheria/runtime/cross_zone.h>

#include "core/rules/ruleset.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <ranges>
#include <vector>

namespace aetheria::local {
namespace {

enum class RayState : std::uint8_t {
  Unchecked,
  Clear,
  Blocked,
  Unknown,
};

[[nodiscard]] constexpr spatial::BoundarySide
horizontal_side(std::int32_t dx) noexcept {
  return dx > 0 ? spatial::BoundarySide::East : spatial::BoundarySide::West;
}

[[nodiscard]] constexpr spatial::BoundarySide
vertical_side(std::int32_t dy) noexcept {
  return dy > 0 ? spatial::BoundarySide::South : spatial::BoundarySide::North;
}

[[nodiscard]] RayState
uncached_edge_state(const runtime::CrossZoneRuntime &runtime,
                    const rules::Ruleset &ruleset, LocalLocation from,
                    spatial::BoundarySide side,
                    const DoorStateQuery &door_states) {
  const auto edge = runtime.peek_edge(from.zone, from.tile, side);
  if (!edge.has_value()) {
    return RayState::Unknown;
  }
  const auto *definition = ruleset.edge(edge->edge);
  if (definition == nullptr) {
    return RayState::Unknown;
  }
  if ((definition->flags & rules::kEdgeWallFlag) == 0) {
    return RayState::Clear;
  }
  if ((definition->flags & rules::kEdgeOpenableFlag) == 0) {
    return RayState::Blocked;
  }
  const auto address = local_edge_address(from, side);
  if (!address.has_value()) {
    return RayState::Unknown;
  }
  return query_door_state(door_states, *address) == DoorState::Open
             ? RayState::Clear
             : RayState::Blocked;
}

// 同一回 FOV 的射線會重複穿越相同有向邊；以相對座標保存查詢結果，
// 避免反覆進入 CrossZoneRuntime，但不把 cache 帶到下一回合。
class EdgeStateCache {
public:
  EdgeStateCache(const runtime::CrossZoneRuntime &runtime,
                 const rules::Ruleset &ruleset,
                 const DoorStateQuery &door_states, std::int32_t radius)
      : runtime_{runtime}, ruleset_{ruleset}, door_states_{door_states},
        radius_{radius}, extent_{static_cast<std::size_t>(radius * 2 + 1)},
        states_(extent_ * extent_ * 4U, RayState::Unchecked) {}

  [[nodiscard]] RayState query(LocalLocation from, std::int32_t relative_x,
                               std::int32_t relative_y,
                               spatial::BoundarySide side) {
    const auto x = static_cast<std::size_t>(relative_x + radius_);
    const auto y = static_cast<std::size_t>(relative_y + radius_);
    auto &state =
        states_[(y * extent_ + x) * 4U + static_cast<std::size_t>(side)];
    if (state == RayState::Unchecked) {
      state = uncached_edge_state(runtime_, ruleset_, from, side, door_states_);
    }
    return state;
  }

private:
  const runtime::CrossZoneRuntime &runtime_;
  const rules::Ruleset &ruleset_;
  const DoorStateQuery &door_states_;
  std::int32_t radius_;
  std::size_t extent_;
  std::vector<RayState> states_;
};

[[nodiscard]] RayState combine_corner(std::array<RayState, 4> states) noexcept {
  if (std::ranges::find(states, RayState::Blocked) != states.end()) {
    return RayState::Blocked;
  }
  return std::ranges::find(states, RayState::Unknown) != states.end()
             ? RayState::Unknown
             : RayState::Clear;
}

[[nodiscard]] RayState line_state(EdgeStateCache &edge_states,
                                  LocalLocation origin, std::int32_t dx,
                                  std::int32_t dy) {
  const auto absolute_dx = std::abs(dx);
  const auto absolute_dy = std::abs(dy);
  const auto side_x = horizontal_side(dx);
  const auto side_y = vertical_side(dy);
  std::int32_t crossed_x{};
  std::int32_t crossed_y{};
  auto current = origin;

  while (crossed_x < absolute_dx || crossed_y < absolute_dy) {
    const auto relative_x = dx < 0 ? -crossed_x : crossed_x;
    const auto relative_y = dy < 0 ? -crossed_y : crossed_y;
    const bool only_x = crossed_y == absolute_dy;
    const bool only_y = crossed_x == absolute_dx;
    const auto x_time =
        static_cast<std::int64_t>(crossed_x * 2 + 1) * absolute_dy;
    const auto y_time =
        static_cast<std::int64_t>(crossed_y * 2 + 1) * absolute_dx;
    const bool cross_x = only_x || (!only_y && x_time < y_time);
    const bool cross_y = only_y || (!only_x && y_time < x_time);

    if (cross_x) {
      const auto state =
          edge_states.query(current, relative_x, relative_y, side_x);
      if (state != RayState::Clear) {
        return state;
      }
      const auto next = adjacent_location(current, side_x);
      if (!next.has_value()) {
        return RayState::Unknown;
      }
      current = *next;
      ++crossed_x;
      continue;
    }
    if (cross_y) {
      const auto state =
          edge_states.query(current, relative_x, relative_y, side_y);
      if (state != RayState::Clear) {
        return state;
      }
      const auto next = adjacent_location(current, side_y);
      if (!next.has_value()) {
        return RayState::Unknown;
      }
      current = *next;
      ++crossed_y;
      continue;
    }

    // 光線正好穿過格角：檢查角上四條邊，反向光線會得到同一集合。
    const auto after_x = adjacent_location(current, side_x);
    const auto after_y = adjacent_location(current, side_y);
    if (!after_x.has_value() || !after_y.has_value()) {
      return RayState::Unknown;
    }
    const auto step_x = dx < 0 ? -1 : 1;
    const auto step_y = dy < 0 ? -1 : 1;
    const auto corner = combine_corner({
        edge_states.query(current, relative_x, relative_y, side_x),
        edge_states.query(current, relative_x, relative_y, side_y),
        edge_states.query(*after_y, relative_x, relative_y + step_y, side_x),
        edge_states.query(*after_x, relative_x + step_x, relative_y, side_y),
    });
    if (corner != RayState::Clear) {
      return corner;
    }
    const auto diagonal = adjacent_location(*after_x, side_y);
    if (!diagonal.has_value()) {
      return RayState::Unknown;
    }
    current = *diagonal;
    ++crossed_x;
    ++crossed_y;
  }
  return RayState::Clear;
}

[[nodiscard]] constexpr std::uint16_t
light_radius(std::uint8_t light, FovParameters parameters) noexcept {
  const auto range = static_cast<std::uint32_t>(parameters.bright_radius -
                                                parameters.dark_radius);
  return static_cast<std::uint16_t>(
      parameters.dark_radius + (range * light + UINT32_C(127)) / UINT32_C(255));
}

} // namespace

FovResult calculate_fov(const runtime::CrossZoneRuntime &runtime,
                        const rules::Ruleset &ruleset, LocalLocation origin,
                        FovParameters parameters,
                        const DoorStateQuery &door_states) {
  FovResult result;
  if (parameters.dark_radius > parameters.bright_radius) {
    return result;
  }
  const auto origin_tile = runtime.peek_tile(origin.zone, origin.tile);
  if (!origin_tile.has_value()) {
    result.degraded = true;
    return result;
  }
  const auto maximum = static_cast<std::int32_t>(parameters.bright_radius);
  EdgeStateCache edge_states{runtime, ruleset, door_states, maximum};
  result.visible.reserve(static_cast<std::size_t>(maximum * 2 + 1) *
                         static_cast<std::size_t>(maximum * 2 + 1));
  for (std::int32_t dy = -maximum; dy <= maximum; ++dy) {
    for (std::int32_t dx = -maximum; dx <= maximum; ++dx) {
      const auto target = offset_location(origin, dx, dy);
      if (!target.has_value()) {
        result.degraded = true;
        continue;
      }
      const auto target_tile = runtime.peek_tile(target->zone, target->tile);
      if (!target_tile.has_value()) {
        result.degraded = true;
        continue;
      }
      const auto radius =
          std::max(light_radius(origin_tile->light, parameters),
                   light_radius(target_tile->light, parameters));
      const auto distance_squared = static_cast<std::int64_t>(dx) * dx +
                                    static_cast<std::int64_t>(dy) * dy;
      if (distance_squared > static_cast<std::int64_t>(radius) * radius) {
        continue;
      }
      const auto state = line_state(edge_states, origin, dx, dy);
      if (state == RayState::Clear) {
        result.visible.push_back(*target);
      } else if (state == RayState::Unknown) {
        result.degraded = true;
      }
    }
  }
  return result;
}

bool is_visible(const FovResult &fov, LocalLocation location) noexcept {
  return std::ranges::find(fov.visible, location) != fov.visible.end();
}

} // namespace aetheria::local
