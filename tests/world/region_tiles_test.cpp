#include "core/world/region_tiles.h"
#include "tests/support/ruleset_fixture.h"

#include <cstddef>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::EdgeId;
using aetheria::tests::test_ruleset;
using aetheria::world::RegionTiles;
using aetheria::world::RegionXY;

TEST(RegionTiles, SetEdgeWritesBothSidesOfTheSameBoundary) {
    RegionTiles tiles{3, 2};
    const auto road = test_ruleset().find_edge("edge.road");
    ASSERT_TRUE(road.has_value());
    constexpr RegionXY left{0, 1};
    constexpr RegionXY right{1, 1};

    tiles.set_edge(left, right, *road);

    EXPECT_EQ(tiles.edge_between(left, right), *road);
    EXPECT_EQ(tiles.edge_between(right, left), *road);
    EXPECT_THROW(tiles.set_edge(left, RegionXY{2, 1}, EdgeId{}), std::runtime_error);
}

TEST(RegionTiles, ReportsActualStorageForDesignedRegionSize) {
    RegionTiles tiles{128, 96};

    EXPECT_TRUE(tiles.valid_layout());
    EXPECT_EQ(tiles.tile_count(), 12'288U);
    EXPECT_EQ(tiles.edge_storage_bytes(), 98'304U);
    RecordProperty("edge_storage_bytes", tiles.edge_storage_bytes());
    RecordProperty("all_soa_storage_bytes", tiles.dynamic_storage_bytes());
    std::cout << "RegionTiles 128x96 edge_bytes=" << tiles.edge_storage_bytes()
              << " all_soa_bytes=" << tiles.dynamic_storage_bytes() << '\n';
}

}  // namespace
