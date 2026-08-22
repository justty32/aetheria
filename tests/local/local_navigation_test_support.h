#pragma once

#include "core/local/local_navigation.h"
#include "core/rules/ruleset.h"
#include "core/zone/zone.h"
#include "core/zone/zone_manager.h"
#include "tests/support/ruleset_fixture.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace aetheria::tests {

inline constexpr auto kNavigationRegion =
    zone::child_key(zone::kRootZone, 17, 0);
inline constexpr auto kNavigationSite =
    zone::child_key(kNavigationRegion, 20, 21);
inline constexpr auto kNavigationCenter =
    zone::child_key(kNavigationSite, 30, 31);
inline constexpr auto kNavigationEast =
    zone::child_key(kNavigationSite, 31, 31);

[[nodiscard]] inline std::unique_ptr<zone::Zone>
navigation_zone(zone::ZoneKey key, std::uint8_t light = UINT8_MAX) {
  auto result = std::make_unique<zone::Zone>(key);
  local::LocalTiles tiles;
  tiles.ground.assign(local::kLocalTileCount,
                      *test_ruleset().find_ground("ground.grass"));
  tiles.overlay.assign(local::kLocalTileCount, local::OverlayId::None);
  tiles.occupant.assign(local::kLocalTileCount, 0);
  tiles.edges.assign(local::kLocalTileCount * 4U,
                     *test_ruleset().find_edge("edge.none"));
  tiles.light.assign(local::kLocalTileCount, light);
  std::get<zone::LocalPayload>(result->payload)
      .layers.emplace(0, std::move(tiles));
  return result;
}

[[nodiscard]] inline std::size_t
navigation_tile_index(local::LocalXY tile) noexcept {
  return static_cast<std::size_t>(tile.y) * local::kLocalWidth + tile.x;
}

[[nodiscard]] inline spatial::BoundarySide
opposite(spatial::BoundarySide side) noexcept {
  switch (side) {
  case spatial::BoundarySide::North:
    return spatial::BoundarySide::South;
  case spatial::BoundarySide::East:
    return spatial::BoundarySide::West;
  case spatial::BoundarySide::South:
    return spatial::BoundarySide::North;
  case spatial::BoundarySide::West:
    return spatial::BoundarySide::East;
  }
  return spatial::BoundarySide::North;
}

inline void set_navigation_edge(zone::Zone &first, local::LocalXY tile,
                                spatial::BoundarySide side, rules::EdgeId edge,
                                zone::Zone *adjacent_zone = nullptr) {
  auto &first_tiles = std::get<zone::LocalPayload>(first.payload).layers.at(0);
  first_tiles.edges[navigation_tile_index(tile) * 4U +
                    static_cast<std::size_t>(side)] = edge;
  const auto adjacent = local::adjacent_location({first.key, tile}, side);
  if (!adjacent.has_value()) {
    return;
  }
  auto *second = adjacent->zone == first.key ? &first : adjacent_zone;
  if (second == nullptr || second->key != adjacent->zone) {
    return;
  }
  auto &second_tiles =
      std::get<zone::LocalPayload>(second->payload).layers.at(0);
  second_tiles.edges[navigation_tile_index(adjacent->tile) * 4U +
                     static_cast<std::size_t>(opposite(side))] = edge;
}

[[nodiscard]] inline local::DoorStateQuery
constant_door_state(local::DoorState state) {
  return [state](const local::LocalEdgeAddress &) { return state; };
}

} // namespace aetheria::tests
