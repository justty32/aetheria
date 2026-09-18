#include "tests/site/site_combat_test_support.h"

#include "core/world/combat_scaling.h"
#include "tests/support/performance.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::site::CohortFacing;
using aetheria::site::CohortFormation;
using aetheria::site::CohortRole;
using aetheria::site::SiteCohort;

using aetheria::tests::site_combat::neutral;

[[nodiscard]] SiteCohort cohort(CohortRole role, std::int16_t x,
                                std::int16_t y) {
    return {.cohort_id = 1,
            .role = role,
            .formation = role == CohortRole::Spear ? CohortFormation::SpearWall
                                                   : CohortFormation::Line,
            .facing = CohortFacing::East,
            .position = {x, y},
            .power = 100'000,
            .formation_integrity_permyriad = 10'000,
            .morale_permyriad = 10'000};
}

TEST(SiteCombat, FifteenMinuteStrideMovesThreeTilesAndPreservesManeuver) {
    EXPECT_EQ(aetheria::site::kSiteEngagementStrideSeconds, 900);
    EXPECT_EQ(aetheria::site::site_engagement_movement_tiles(), 3);
    const auto turns_to_cross =
        (63 + aetheria::site::site_engagement_movement_tiles() - 1) /
        aetheria::site::site_engagement_movement_tiles();
    EXPECT_EQ(turns_to_cross, 21);
    EXPECT_GT(turns_to_cross, aetheria::site::kSiteCombatTurns);
    std::cout << "site_stride seconds=900 metres_per_tile=125 speed_mph=1500"
              << " movement_tiles=3 turns_to_cross_63=21 battle_turns=16\n";
}

TEST(SiteCombat, EveryMatchupBonusRequiresItsTacticalCondition) {
    const auto modifiers = neutral();
    std::array<std::int32_t, 4> met{};
    std::array<std::int32_t, 4> unmet{};

    auto spear = cohort(CohortRole::Spear, 10, 10);
    const auto cavalry = cohort(CohortRole::Cavalry, 11, 10);
    met[0] = aetheria::site::resolve_site_attack(spear, cavalry, modifiers, modifiers)
                 .final_loss;
    spear.formation_integrity_permyriad = 7'999;
    unmet[0] = aetheria::site::resolve_site_attack(spear, cavalry, modifiers, modifiers)
                   .final_loss;

    auto flanking_cavalry = cohort(CohortRole::Cavalry, 10, 9);
    auto archer = cohort(CohortRole::Archer, 10, 10);
    met[1] = aetheria::site::resolve_site_attack(flanking_cavalry, archer, modifiers,
                                                modifiers)
                 .final_loss;
    flanking_cavalry.position = {11, 10};
    unmet[1] = aetheria::site::resolve_site_attack(flanking_cavalry, archer, modifiers,
                                                  modifiers)
                   .final_loss;

    archer = cohort(CohortRole::Archer, 10, 10);
    spear = cohort(CohortRole::Spear, 14, 10);
    met[2] = aetheria::site::resolve_site_attack(archer, spear, modifiers, modifiers)
                 .final_loss;
    spear.position = {13, 10};
    unmet[2] = aetheria::site::resolve_site_attack(archer, spear, modifiers, modifiers)
                   .final_loss;

    auto siege = cohort(CohortRole::Siege, 10, 10);
    auto wall = cohort(CohortRole::Spear, 19, 10);
    siege.in_cover = true;
    wall.behind_wall = true;
    met[3] = aetheria::site::resolve_site_attack(siege, wall, modifiers, modifiers)
                 .final_loss;
    siege.moved_tiles_last_turn = 1;
    unmet[3] = aetheria::site::resolve_site_attack(siege, wall, modifiers, modifiers)
                   .final_loss;

    EXPECT_EQ(met, (std::array<std::int32_t, 4>{1'500, 1'600, 1'400, 1'800}));
    EXPECT_EQ(unmet, (std::array<std::int32_t, 4>{1'000, 1'000, 1'000, 1'000}));
    for (std::size_t index = 0; index < met.size(); ++index) {
        EXPECT_GT(met[index], unmet[index]);
    }
    std::cout << "site_matchups spear_vs_cavalry=" << met[0] << '/' << unmet[0]
              << " cavalry_vs_archer=" << met[1] << '/' << unmet[1]
              << " archer_vs_spear=" << met[2] << '/' << unmet[2]
              << " siege_vs_wall=" << met[3] << '/' << unmet[3]
              << " false_samples_each=1\n";
}

TEST(SiteCombat, ExistingTerrainAndCommandModifiersAffectCohortExchange) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    auto attacker = cohort(CohortRole::Spear, 10, 10);
    const auto defender = cohort(CohortRole::Archer, 11, 10);
    auto attacker_modifiers = neutral();
    auto defender_modifiers = neutral();
    const auto baseline = aetheria::site::resolve_site_attack(
        attacker, defender, attacker_modifiers, defender_modifiers);
    attacker_modifiers.command = 1'200;
    const auto commanded = aetheria::site::resolve_site_attack(
        attacker, defender, attacker_modifiers, defender_modifiers,
        rules.modifier_scale);
    defender_modifiers.terrain = 1'500;
    const auto fortified = aetheria::site::resolve_site_attack(
        attacker, defender, attacker_modifiers, defender_modifiers,
        rules.modifier_scale);
    EXPECT_EQ(rules.modifier_scale, 1'000);
    EXPECT_EQ(baseline.final_loss, 1'000);
    EXPECT_EQ(commanded.final_loss, 1'200);
    EXPECT_EQ(fortified.final_loss, 800);
    std::cout << "site_modifiers baseline=1000 command_1200=1200"
              << " command_1200_vs_terrain_1500=800\n";
}

}  // namespace
