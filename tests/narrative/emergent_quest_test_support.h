#pragma once

#include "core/narrative/emergent_quest.h"

#include "core/serialize/zone_codec.h"
#include "core/site/site_build_loop.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "tests/site/site_reduction_test_support.h"

#include <array>
#include <iostream>
#include <ranges>

#include <gtest/gtest.h>

namespace aetheria::tests::emergent_quest {

using aetheria::narrative::EmergentQuestKind;
using aetheria::narrative::NarrativeWorldSnapshot;
using aetheria::narrative::NarrativeWorldView;
using aetheria::tests::kReductionCoordinate;
using aetheria::tests::kReductionRegionId;
using aetheria::tests::kReductionWorldSeed;
using aetheria::tests::reduction_region;
using aetheria::tests::test_ruleset;

[[nodiscard]] inline NarrativeWorldSnapshot meaningful_needs() {
    NarrativeWorldSnapshot state;
    state.cities = {
        {kReductionCoordinate, "place.stonebridge", 40, 100},
        {{0, 1}, "place.full_granary", 100, 100},
    };
    state.tiles = {
        {{4, 5}, "place.old_road", 20, 60},
        {{5, 5}, "place.safe_road", 60, 60},
    };
    state.named_npcs = {
        {41, "person.martha_grocer", "place.stonebridge", true, true, true},
        {42, "person.unknown", "place.stonebridge", false, true, true},
        {43, "person.glen_veteran", "place.stonebridge", true, true, false},
    };
    state.faction_tensions = {
        {1, 2, "faction.ember", "faction.river", 800, 700, 1000, 920, 15},
        {1, 3, "faction.ember", "faction.mountain", 900, 700, 1000, 400, 15},
    };
    state.dungeons = {
        {91, "place.deep_ruin", false, 8, 6},
        {92, "place.cleared_ruin", true, 9, 6},
        {93, "place.shallow_cave", false, 2, 6},
    };
    return state;
}

inline void install_real_observations(aetheria::zone::Zone& live_site) {
    const auto& ruleset = test_ruleset();
    auto& persistent = std::get<aetheria::zone::SitePayload>(live_site.payload).layers.persistent;
    persistent.place_name_key = "place.stonebridge";
    persistent.named_npcs = {
        {41, "person.martha_grocer", "place.stonebridge", true, true},
        {42, "person.unknown", "place.stonebridge", false, true},
        {43, "person.glen_veteran", "place.stonebridge", true, false},
    };
    const auto dungeon_id = ruleset.find_building("building.dungeon_entrance");
    ASSERT_TRUE(dungeon_id.has_value());
    const auto* dungeon_definition = ruleset.building(*dungeon_id);
    ASSERT_NE(dungeon_definition, nullptr);
    const auto minimum_depth = ruleset.world_observation_rules().dungeon_minimum_depth;
    ASSERT_GT(minimum_depth, 1U);
    persistent.dungeons = {
        {91, "place.deep_ruin", false, dungeon_definition->underground_depth},
        {92, "place.cleared_ruin", true, dungeon_definition->underground_depth},
        {93, "place.shallow_cave", false, static_cast<std::uint16_t>(minimum_depth - 1U)},
    };
}

}  // namespace aetheria::tests::emergent_quest
