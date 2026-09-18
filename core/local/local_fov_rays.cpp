// local_fov_rays.cpp：整數 DDA 穿邊射線與本回合邊狀態快取。

#include "core/local/local_fov_detail.h"

#include <aetheria/runtime/cross_zone.h>

#include "core/rules/ruleset.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <ranges>
#include <vector>

namespace aetheria::local::detail::fov {

enum class RayState : std::uint8_t {
  Unchecked,
  Clear,
  Blocked,
  Unknown,
};

namespace {

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

} // namespace

EdgeStateCache::EdgeStateCache(const runtime::CrossZoneRuntime &runtime,
                 const rules::Ruleset &ruleset,
                 const DoorStateQuery &door_states, std::int32_t radius)
      : runtime_{runtime}, ruleset_{ruleset}, door_states_{door_states},
        radius_{radius}, extent_{static_cast<std::size_t>(radius * 2 + 1)},
        states_(extent_ * extent_ * 4U, RayState::Unchecked) {}

RayState EdgeStateCache::query(LocalLocation from, std::int32_t relative_x,
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

void append_visible_ray(FovResult &result, EdgeStateCache &edge_states,
                        LocalLocation origin, std::int32_t dx,
                        std::int32_t dy, LocalLocation target) {
  const auto state = line_state(edge_states, origin, dx, dy);
  if (state == RayState::Clear) {
    result.visible.push_back(target);
  } else if (state == RayState::Unknown) {
    result.degraded = true;
  }
}

} // namespace aetheria::local::detail::fov
