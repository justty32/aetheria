// local_fov.cpp：依光照半徑掃描候選格並收集可見位置。

#include "core/local/local_fov_detail.h"

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

[[nodiscard]] constexpr std::uint16_t
light_radius(std::uint8_t light, FovParameters parameters) noexcept {
  const auto range = static_cast<std::uint32_t>(parameters.bright_radius -
                                                parameters.dark_radius);
  return static_cast<std::uint16_t>(
      parameters.dark_radius + (range * light + UINT32_C(127)) / UINT32_C(255));
}

} // namespace

using namespace detail::fov;

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
      append_visible_ray(result, edge_states, origin, dx, dy, *target);
    }
  }
  return result;
}

bool is_visible(const FovResult &fov, LocalLocation location) noexcept {
  return std::ranges::find(fov.visible, location) != fov.visible.end();
}

} // namespace aetheria::local
