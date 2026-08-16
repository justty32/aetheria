#pragma once

// tests/world 底下 region 移動／路徑／旬回合測試共用的 fixture：
// 純地形 tiles、佔位 zone meta 與含兩個單位的 movement zone。

#include "core/world/region_movement.h"
#include "core/zone/zone_key.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include <gtest/gtest.h>

namespace aetheria::tests {

using aetheria::rules::Ruleset;
using aetheria::world::MovementPoints;
using aetheria::world::RegionMoveCommand;
using aetheria::world::RegionPosition;
using aetheria::world::RegionTiles;
using aetheria::world::RegionXY;
using aetheria::world::StableId;
using aetheria::world::TurnClock;
using aetheria::zone::Zone;

[[nodiscard]] inline RegionTiles plain_tiles(std::uint32_t width, std::uint32_t height,
                                             const Ruleset& ruleset = test_ruleset()) {
    RegionTiles tiles{width, height};
    std::fill(tiles.base.begin(), tiles.base.end(), *ruleset.find_terrain("terrain.grassland"));
    std::fill(tiles.relief.begin(), tiles.relief.end(), *ruleset.find_relief("relief.plain"));
    std::fill(tiles.feature.begin(), tiles.feature.end(), *ruleset.find_feature("feature.none"));
    std::fill(tiles.edges.begin(), tiles.edges.end(), *ruleset.find_edge("edge.none"));
    return tiles;
}

[[nodiscard]] inline entt::entity placeholder(Zone& zone) {
    const auto meta = zone.reg.view<aetheria::zone::ZoneMeta>();
    EXPECT_EQ(meta.size(), 1U);
    return *meta.begin();
}

inline void add_unit(Zone& zone, StableId stable_id, RegionXY tile, RegionXY target) {
    const auto entity = zone.reg.create();
    zone.reg.emplace<StableId>(entity, stable_id);
    zone.reg.emplace<RegionPosition>(entity, 0, tile);
    zone.reg.emplace<MovementPoints>(entity, 0, 4);
    zone.reg.emplace<RegionMoveCommand>(entity, target, false);
}

[[nodiscard]] inline std::unique_ptr<Zone> movement_zone(bool reverse_units = false) {
    const auto key = aetheria::zone::child_key(aetheria::zone::kRootZone, 1, 0);
    auto zone = std::make_unique<Zone>(key);
    std::get<aetheria::zone::RegionPayload>(zone->payload).layers.emplace(0, plain_tiles(12, 1));
    zone->reg.emplace<TurnClock>(placeholder(*zone), aetheria::time::Tick{0});
    if (reverse_units) {
        add_unit(*zone, StableId{20}, RegionXY{11, 0}, RegionXY{0, 0});
        add_unit(*zone, StableId{10}, RegionXY{0, 0}, RegionXY{11, 0});
    } else {
        add_unit(*zone, StableId{10}, RegionXY{0, 0}, RegionXY{11, 0});
        add_unit(*zone, StableId{20}, RegionXY{11, 0}, RegionXY{0, 0});
    }
    return zone;
}

}  // namespace aetheria::tests
