#include "core/worldgen/portal_candidates.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::rules::RulesetLoader;
using aetheria::rules::WorldConnectionType;
using aetheria::tests::copy_data_files;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::tests::write_text;
using aetheria::world::RegionPortal;
using aetheria::world::RegionTiles;
using aetheria::world::RegionXY;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::RegionBuildResult;
using aetheria::worldgen::RegionSlowVariables;

constexpr RegionSlowVariables kSlow{0, 128, 96};
constexpr auto kSeed = UINT64_C(12345);

[[nodiscard]] const RegionBuildResult& real_region() {
    static const auto result = build_skeleton(kSlow, kSeed, test_ruleset());
    return result;
}

[[nodiscard]] const RegionPortal& portal_of_type(const RegionBuildResult& result,
                                                 WorldConnectionType type) {
    for (const auto& portal : result.portals.portals) {
        const auto connection =
            std::ranges::find(test_ruleset().world_connections(), portal.channel,
                              &aetheria::rules::WorldGraphConnection::id);
        if (connection != test_ruleset().world_connections().end() && connection->type == type) {
            return portal;
        }
    }
    throw std::runtime_error{"portal collision fixture missing connection type"};
}

[[nodiscard]] std::int64_t move_cost(const RegionTiles& tiles, std::size_t index) {
    const auto* terrain = test_ruleset().terrain(tiles.base.at(index));
    const auto* relief = test_ruleset().relief(tiles.relief.at(index));
    const auto* feature = test_ruleset().feature(tiles.feature.at(index));
    if (terrain == nullptr || relief == nullptr || feature == nullptr) {
        throw std::runtime_error{"portal collision metric invalid definition"};
    }
    return static_cast<std::int64_t>(terrain->move_cost) + relief->move_cost + feature->move_cost;
}

[[nodiscard]] Ruleset duplicate_teleport_rules(RegionXY tile) {
    TemporaryDirectory directory;
    copy_data_files(directory.path());
    std::ostringstream graph;
    graph << "regions = [0, 1, 2]\n\n"
          << "[[connections]]\n"
          << "id = 1\nregion_a = 0\nregion_b = 1\ntype = \"teleport\"\n"
          << "cost_ticks = 1\nrequirement = \"none\"\ncoordinate_a = [" << tile.x << ", " << tile.y
          << "]\n\n"
          << "[[connections]]\n"
          << "id = 2\nregion_a = 0\nregion_b = 2\ntype = \"teleport\"\n"
          << "cost_ticks = 1\nrequirement = \"none\"\ncoordinate_a = [" << tile.x << ", " << tile.y
          << "]\n";
    write_text(directory.path() / "world_graph.toml", graph.str());
    return RulesetLoader::load(directory.path());
}

TEST(PortalCollision, RealRegionTilesAreDistinctAndCriteriaAreMeasured) {
    const auto& result = real_region();
    const auto tiles = aetheria::worldgen::populate(result.skeleton, {});
    std::set<std::size_t> occupied;
    for (const auto& portal : result.portals.portals) {
        EXPECT_TRUE(occupied.insert(tiles.index_of(portal.tile)).second);
    }

    const auto& mountain_pass = portal_of_type(result, WorldConnectionType::MountainPass);
    const auto& underground = portal_of_type(result, WorldConnectionType::Underground);
    const auto pass_index = tiles.index_of(mountain_pass.tile);
    const auto underground_index = tiles.index_of(underground.tile);
    const auto mountain = test_ruleset().find_relief("relief.mountain");
    ASSERT_TRUE(mountain.has_value());
    const auto* underground_feature = test_ruleset().feature(tiles.feature[underground_index]);
    ASSERT_NE(underground_feature, nullptr);
    EXPECT_NE(pass_index, underground_index);
    std::cout << "portal_collision mountain_tile=" << mountain_pass.tile.x << ','
              << mountain_pass.tile.y << " cost=" << move_cost(tiles, pass_index)
              << " relief=" << aetheria::rules::value_of(tiles.relief[pass_index])
              << " mountain_reason="
              << (tiles.relief[pass_index] == *mountain ? "easiest_boundary_mountain"
                                                        : "boundary_land_fallback")
              << " underground_tile=" << underground.tile.x << ',' << underground.tile.y
              << " cost=" << move_cost(tiles, underground_index)
              << " relief=" << aetheria::rules::value_of(tiles.relief[underground_index])
              << " underground_reason="
              << ((underground_feature->flags & aetheria::rules::kFeatureRuinFlag) != 0
                      ? "boundary_ruin"
                  : tiles.relief[underground_index] == *mountain ? "deepest_boundary_mountain"
                                                                 : "boundary_land_fallback")
              << '\n';
}

TEST(PortalCollision, SyntheticMountainsUseLowestForPassAndHighestForUnderground) {
    RegionTiles tiles{5, 3};
    const auto grassland = test_ruleset().find_terrain("terrain.grassland");
    const auto plain = test_ruleset().find_relief("relief.plain");
    const auto mountain = test_ruleset().find_relief("relief.mountain");
    const auto none = test_ruleset().find_feature("feature.none");
    const auto mine = test_ruleset().find_feature("feature.mine");
    const auto ruin = test_ruleset().find_feature("feature.ruin_village");
    ASSERT_TRUE(grassland.has_value());
    ASSERT_TRUE(plain.has_value());
    ASSERT_TRUE(mountain.has_value());
    ASSERT_TRUE(none.has_value());
    ASSERT_TRUE(mine.has_value());
    ASSERT_TRUE(ruin.has_value());
    std::ranges::fill(tiles.base, *grassland);
    std::ranges::fill(tiles.relief, *plain);
    std::ranges::fill(tiles.feature, *none);
    tiles.relief[0] = *mountain;
    tiles.relief[4] = *mountain;
    tiles.relief[14] = *mountain;
    tiles.feature[4] = *mine;
    tiles.feature[14] = *mine;
    std::vector<std::uint8_t> occupied(tiles.tile_count());

    const auto mountain_pass =
        aetheria::worldgen::detail::resolve_boundary_portal(tiles, test_ruleset(), false, occupied);
    occupied[mountain_pass] = 1;
    const auto underground =
        aetheria::worldgen::detail::resolve_boundary_portal(tiles, test_ruleset(), true, occupied);

    EXPECT_EQ(mountain_pass, 0U);
    EXPECT_EQ(underground, 4U);

    std::ranges::fill(occupied, std::uint8_t{});
    tiles.feature[10] = *ruin;
    const auto ruin_underground =
        aetheria::worldgen::detail::resolve_boundary_portal(tiles, test_ruleset(), true, occupied);
    EXPECT_EQ(ruin_underground, 10U);
}

TEST(PortalCollision, DuplicateSpecifiedTileFailsFast) {
    const auto& result = real_region();
    const auto& teleport = portal_of_type(result, WorldConnectionType::Teleport);
    const auto ruleset = duplicate_teleport_rules(teleport.tile);

    EXPECT_THROW(static_cast<void>(build_skeleton(kSlow, kSeed, ruleset)), std::runtime_error);
}

}  // namespace
