#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::worldgen::hash_stage;
using aetheria::worldgen::RegionBuildResult;

[[nodiscard]] std::array<std::uint64_t, 10>
early_stage_hashes(const RegionBuildResult& result) {
    return {hash_stage(result.plates),   hash_stage(result.height),
            hash_stage(result.erosion),  hash_stage(result.climate),
            hash_stage(result.rivers),   hash_stage(result.biome),
            hash_stage(result.features), hash_stage(result.history),
            hash_stage(result.cities),   hash_stage(result.roads)};
}

TEST(LateStageIsolation, PortalAndFactionParametersCannotChangeStagesOneThroughTen) {
    constexpr aetheria::worldgen::RegionSlowVariables slow{0, 128, 96};
    constexpr auto seed = UINT64_C(12345);
    aetheria::worldgen::RegionGenerationConfig portal_config;
    aetheria::worldgen::RegionGenerationConfig faction_config;
    portal_config.portals.road_tier = 1;
    faction_config.factions.first_faction_id = 7;

    const auto baseline = aetheria::worldgen::build_skeleton(
        slow, seed, aetheria::tests::test_ruleset());
    const auto changed_portals = aetheria::worldgen::build_skeleton(
        slow, seed, aetheria::tests::test_ruleset(), portal_config);
    const auto changed_factions = aetheria::worldgen::build_skeleton(
        slow, seed, aetheria::tests::test_ruleset(), faction_config);
    const auto hashes = early_stage_hashes(baseline);

    EXPECT_EQ(hashes, early_stage_hashes(changed_portals));
    EXPECT_EQ(hashes, early_stage_hashes(changed_factions));
    EXPECT_NE(hash_stage(baseline.portals), hash_stage(changed_portals.portals));
    EXPECT_NE(hash_stage(baseline.factions), hash_stage(changed_factions.factions));

    std::cout << "late_stage_early_hashes=";
    for (std::size_t index = 0; index < hashes.size(); ++index) {
        std::cout << (index == 0 ? "" : ",") << hashes[index];
    }
    std::cout << '\n';
}

}  // namespace
