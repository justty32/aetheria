// local_navigation.cpp：Local 格位址跨 64×64 zone 邊界的決定性換算。

#include "core/local/local_navigation.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace aetheria::local {
namespace {

inline constexpr std::int64_t kLocalZoneAxisCount = 1'024;
inline constexpr std::int64_t kSiteLocalTileExtent =
    kLocalZoneAxisCount * static_cast<std::int64_t>(kLocalWidth);

} // namespace

std::optional<LocalLocation> offset_location(LocalLocation origin,
                                             std::int32_t dx,
                                             std::int32_t dy) noexcept {
  if (zone::level_of(origin.zone) != zone::ZoneLevel::Local ||
      origin.tile.x >= kLocalWidth || origin.tile.y >= kLocalHeight) {
    return std::nullopt;
  }
  const auto global_x =
      static_cast<std::int64_t>(zone::local_x_of(origin.zone)) * kLocalWidth +
      origin.tile.x + dx;
  const auto global_y =
      static_cast<std::int64_t>(zone::local_y_of(origin.zone)) * kLocalHeight +
      origin.tile.y + dy;
  if (global_x < 0 || global_y < 0 || global_x >= kSiteLocalTileExtent ||
      global_y >= kSiteLocalTileExtent) {
    return std::nullopt;
  }
  const auto zone_x = static_cast<std::uint32_t>(global_x / kLocalWidth);
  const auto zone_y = static_cast<std::uint32_t>(global_y / kLocalHeight);
  return LocalLocation{
      zone::child_key(zone::parent_of(origin.zone), zone_x, zone_y),
      {static_cast<std::uint16_t>(global_x % kLocalWidth),
       static_cast<std::uint16_t>(global_y % kLocalHeight)}};
}

std::optional<LocalLocation>
adjacent_location(LocalLocation origin,
                  spatial::BoundarySide direction) noexcept {
  switch (direction) {
  case spatial::BoundarySide::North:
    return offset_location(origin, 0, -1);
  case spatial::BoundarySide::East:
    return offset_location(origin, 1, 0);
  case spatial::BoundarySide::South:
    return offset_location(origin, 0, 1);
  case spatial::BoundarySide::West:
    return offset_location(origin, -1, 0);
  }
  return std::nullopt;
}

std::optional<LocalEdgeAddress>
local_edge_address(LocalLocation origin,
                   spatial::BoundarySide direction) noexcept {
  const auto adjacent = adjacent_location(origin, direction);
  if (!adjacent.has_value()) {
    return std::nullopt;
  }
  const auto [first, second] = std::minmax(origin, *adjacent);
  return LocalEdgeAddress{first, second};
}

DoorState query_door_state(const DoorStateQuery &query,
                           const LocalEdgeAddress &edge) {
  return query ? query(edge) : DoorState::Closed;
}

} // namespace aetheria::local
