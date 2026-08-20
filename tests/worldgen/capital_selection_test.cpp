#include "core/worldgen/influence_spread.h"

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::worldgen::CitySite;

TEST(InfluenceCapitalSelection, IsCanonicalUnderShuffleAndRejectsTooManyFactions) {
    std::vector<CitySite> cities{
        {40, {4, 4}, 80, aetheria::world::SettlementTier::Town, 2},
        {20, {0, 8}, 90, aetheria::world::SettlementTier::City, 4},
        {30, {8, 8}, 90, aetheria::world::SettlementTier::City, 4},
        {10, {8, 0}, 100, aetheria::world::SettlementTier::City, 4},
        {5, {0, 0}, 100, aetheria::world::SettlementTier::City, 4},
    };
    const auto selected_a = aetheria::worldgen::select_capitals(cities, 3);
    std::ranges::reverse(cities);
    const auto selected_b = aetheria::worldgen::select_capitals(cities, 3);

    ASSERT_EQ(selected_a, selected_b);
    ASSERT_EQ(selected_a.size(), 3U);
    EXPECT_EQ(selected_a[0].canonical_id, 5U);
    EXPECT_EQ(selected_a[1].canonical_id, 30U);
    EXPECT_EQ(selected_a[2].canonical_id, 10U);
    EXPECT_THROW(static_cast<void>(aetheria::worldgen::select_capitals(cities, 5)),
                 std::invalid_argument);
}

}  // namespace
