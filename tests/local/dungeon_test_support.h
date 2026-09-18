#pragma once

#include "core/local/dungeon.h"
#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/zone_codec.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/performance.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>

namespace aetheria::tests::dungeon_support {

using aetheria::local::DungeonGenerated;
using aetheria::local::DungeonPersistentState;
using aetheria::rules::DungeonArchetype;
using aetheria::tests::test_ruleset;

[[nodiscard]] inline aetheria::site::PersistentDungeon persistent(std::uint16_t depth = 6,
                                                            bool cleared = false) {
    return {UINT64_C(0xD006E001), "place.test_dungeon", cleared, depth};
}

[[nodiscard]] inline DungeonGenerated generate(std::uint64_t seed = UINT64_C(0xD06E0A),
                                        DungeonArchetype archetype = DungeonArchetype::Hybrid,
                                        std::uint16_t depth = 6, bool cleared = false,
                                        const DungeonPersistentState& state = {}) {
    return aetheria::local::generate_dungeon(seed, archetype, persistent(depth, cleared), state,
                                             test_ruleset());
}

} // namespace aetheria::tests::dungeon_support
