#include "core/runtime/playable_session.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <iostream>

namespace {

using aetheria::runtime::PlayableResidence;
using aetheria::runtime::PlayableSession;

[[nodiscard]] PlayableSession session() {
    return PlayableSession{515151, 51, AETHERIA_SOURCE_DIR "/data"};
}

TEST(PlayableCoverage, ThreeLayersReturnAndWriteAuthoritativeParentValues) {
    auto value = session();
    const auto initial = value.coverage_summary();
    EXPECT_EQ(initial.residence, PlayableResidence::Region);
    EXPECT_EQ(initial.development, 1U);
    EXPECT_EQ(initial.order, 20U);
    EXPECT_TRUE(initial.bandit_quest_available);
    EXPECT_TRUE(initial.dungeon_quest_available);

    value.measure_site_roundtrips();
    const auto roundtrips = value.coverage_summary().roundtrip_hashes;
    ASSERT_EQ(roundtrips.size(), 3U);
    EXPECT_TRUE(std::ranges::all_of(roundtrips, [&](std::uint64_t hash) {
        return hash == roundtrips.front();
    }));

    value.enter_site();
    ASSERT_TRUE(value.site_view().has_value());
    ASSERT_EQ(value.site_view()->cells.size(), 4096U);
    value.build_city();
    auto after_build = value.coverage_summary();
    EXPECT_EQ(after_build.last_development_before, 1U);
    EXPECT_EQ(after_build.last_development_after, 3U);
    EXPECT_EQ(after_build.development, 3U);
    EXPECT_EQ(after_build.city_buildings, 1U);

    value.accept_bandit_quest();
    EXPECT_TRUE(value.coverage_summary().bandit_quest_accepted);
    value.enter_local();
    ASSERT_TRUE(value.local_view().has_value());
    EXPECT_EQ(value.local_view()->z, 0);
    EXPECT_EQ(value.local_view()->cells.size(), 4096U);
    value.open_door_and_move();
    EXPECT_EQ(value.local_view()->player_x, 32U);
    value.suppress_bandits();
    const auto after_suppression = value.coverage_summary();
    EXPECT_EQ(after_suppression.last_order_before, 20U);
    EXPECT_EQ(after_suppression.last_order_after, 50U);
    EXPECT_EQ(after_suppression.order, 50U);
    EXPECT_FALSE(after_suppression.bandit_quest_available);
    EXPECT_FALSE(after_suppression.bandit_quest_accepted);

    value.enter_dungeon();
    EXPECT_EQ(value.local_view()->z, -1);
    value.descend_dungeon();
    value.descend_dungeon();
    EXPECT_EQ(value.local_view()->z, -3);
    value.clear_dungeon();
    const auto after_dungeon = value.coverage_summary();
    EXPECT_TRUE(after_dungeon.dungeon_cleared);
    EXPECT_FALSE(after_dungeon.dungeon_quest_available);
    EXPECT_LT(after_dungeon.dungeon_density_after,
              after_dungeon.dungeon_density_before);

    value.leave_dungeon();
    value.leave_local();
    value.leave_site();
    value.enter_site();
    value.enter_local();
    value.enter_dungeon();
    const auto reentered = value.coverage_summary();
    EXPECT_TRUE(reentered.dungeon_cleared);
    EXPECT_LT(reentered.dungeon_density_after, reentered.dungeon_density_before);
    value.leave_dungeon();
    value.leave_local();
    value.leave_site();
    value.sign_peace_treaty();
    EXPECT_EQ(value.coverage_summary().treaty_count, 1U);

    std::cout << "playable_coverage development="
              << after_build.last_development_before << "->"
              << after_build.last_development_after << " order="
              << after_suppression.last_order_before << "->"
              << after_suppression.last_order_after << " dungeon_density="
              << after_dungeon.dungeon_density_before << "->"
              << after_dungeon.dungeon_density_after << " roundtrip_hashes="
              << roundtrips[0] << '/' << roundtrips[1] << '/' << roundtrips[2]
              << " treaty_count=1\n";
}

TEST(PlayableCoverage, ManualAndManagedCityExpectationMatchesAtOneHundred) {
    auto value = session();
    value.measure_city_management(100);
    const auto measured = value.coverage_summary();
    EXPECT_EQ(measured.calibration_n, 100U);
    EXPECT_GT(measured.manual_total, 0U);
    EXPECT_EQ(measured.manual_total, measured.managed_total);
    EXPECT_DOUBLE_EQ(measured.signed_relative_error_percent, 0.0);
    std::cout << "playable_city_calibration N=" << measured.calibration_n
              << " manual_total=" << measured.manual_total
              << " managed_total=" << measured.managed_total
              << " signed_relative_error_percent=+"
              << measured.signed_relative_error_percent << '\n';
}

} // namespace
