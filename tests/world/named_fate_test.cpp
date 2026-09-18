#include "core/rules/attributes.h"
#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/zone_codec.h"
#include "core/site/site_build_loop.h"
#include "core/site/site_reduction.h"
#include "core/world/named_fate.h"
#include "core/zone/zone.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

#include "tests/world/named_fate_test_support.h"

namespace {

using aetheria::world::FateCrisis;
using aetheria::world::FateExecutionCounters;
using aetheria::world::FateOutcome;
using aetheria::world::FateResolver;
using aetheria::world::FateRules;
using aetheria::world::FateStageOne;
using aetheria::world::NamedFateLedger;
using aetheria::world::NamedFateMember;
using aetheria::world::PopulationReduction;
using aetheria::world::RegionTiles;
using aetheria::world::RegionXY;

using namespace aetheria::tests::named_fate;

constexpr RegionXY kCoordinate{};

[[nodiscard]] RegionTiles region_with_population(std::uint32_t population) {
    RegionTiles tiles{1, 1};
    auto site = make_site_zone(population);
    aetheria::site::ReductionTable::apply(
        tiles, kCoordinate,
        aetheria::site::ReductionTable::reduce(site, aetheria::tests::test_ruleset()));
    return tiles;
}

TEST(NamedFate, MarkingFiftyCannotChangeAuthoritativeCohortLossAndQuotaIsConserved) {
    auto no_mark_tiles = region_with_population(10'000);
    auto marked_tiles = region_with_population(10'000);
    const auto no_mark_stage = FateResolver::apply_stage_one(no_mark_tiles, kCoordinate, crisis(1));
    const auto marked_stage = FateResolver::apply_stage_one(marked_tiles, kCoordinate, crisis(1));
    NamedFateLedger no_mark;
    NamedFateLedger marked;
    for (std::uint64_t uid = 1; uid <= 50; ++uid) {
        marked.members.push_back(member(uid, true));
    }
    FateExecutionCounters counters;
    const auto zero = FateResolver::resolve_present(no_mark, no_mark_stage, FateRules{}, counters);
    const auto fifty = FateResolver::resolve_present(marked, marked_stage, FateRules{}, counters);

    EXPECT_EQ(no_mark_stage.total_loss, 1'200U);
    EXPECT_EQ(marked_stage.total_loss, 1'200U);
    EXPECT_EQ(no_mark_stage.total_loss, marked_stage.total_loss);
    EXPECT_EQ(zero.named_deaths + zero.unnamed_deaths, zero.stage_one_total_loss);
    EXPECT_EQ(fifty.named_deaths + fifty.unnamed_deaths, fifty.stage_one_total_loss);
    EXPECT_EQ(marked.members.size(), 50U);
    EXPECT_GT(fifty.named_deaths, 0U) << "非空具名 cohort 必須真的命中死亡";
    std::cout << "named_fate_antibrush mark0_total=" << no_mark_stage.total_loss
              << " mark50_total=" << marked_stage.total_loss
              << " named_members=" << marked.members.size()
              << " named_deaths=" << fifty.named_deaths
              << " unnamed_deaths=" << fifty.unnamed_deaths << '\n';
}

TEST(NamedFate, ReliefChangesStageOneRegionPopulationRatherThanIndividualModifier) {
    auto baseline = region_with_population(10'000);
    auto relieved = region_with_population(10'000);
    const auto before = baseline.reduction_value<PopulationReduction>(kCoordinate);
    const auto ordinary = FateResolver::apply_stage_one(baseline, kCoordinate, crisis(2));
    const auto intervention =
        FateResolver::apply_stage_one(relieved, kCoordinate, crisis(2, 1'200, 5'000));
    EXPECT_EQ(before, 10'000U);
    EXPECT_EQ(ordinary.population_after, 8'800U);
    EXPECT_EQ(intervention.population_after, 9'400U);
    EXPECT_GT(intervention.population_after, ordinary.population_after);
    EXPECT_EQ(relieved.reduction_value<PopulationReduction>(kCoordinate), 9'400U);
    const auto named = member(501);
    EXPECT_EQ(aetheria::world::adjusted_death_basis_points(ordinary, named, FateRules{}),
              1'200U);
    EXPECT_EQ(aetheria::world::adjusted_death_basis_points(intervention, named, FateRules{}),
              600U);
    std::cout << "named_fate_relief before=" << before
              << " without_relief=" << ordinary.population_after
              << " with_relief=" << intervention.population_after << '\n';
}

TEST(NamedFate, OverflowStopsUnnamedLossAndEmitsExceptionalNarrative) {
    NamedFateLedger ledger;
    for (std::uint64_t uid = 1; uid <= 3; ++uid) {
        auto named = member(uid);
        named.modifiers = {50'000, 50'000, 50'000, 50'000, 50'000, 50'000};
        ledger.members.push_back(std::move(named));
    }
    FateExecutionCounters counters;
    const FateStageOne tiny_quota{crisis(3, 100), 100, 1, 99};
    const auto result =
        FateResolver::resolve_present(ledger, tiny_quota, FateRules{}, counters);
    EXPECT_EQ(result.named_deaths, 3U);
    EXPECT_EQ(result.unnamed_deaths, 0U);
    EXPECT_TRUE(result.named_quota_overflow);
    ASSERT_EQ(ledger.events.size(), 4U);
    EXPECT_EQ(ledger.events.back().kind,
              aetheria::world::FateNarrativeKind::NamedQuotaOverflow);
    std::cout << "named_fate_overflow quota=" << result.stage_one_total_loss
              << " named_deaths=" << result.named_deaths
              << " unnamed_deaths=" << result.unnamed_deaths
              << " overflow_events=1\n";
}

TEST(NamedFate, RescuedNamedMemberTransfersTheQuotaToUnnamedMembers) {
    auto saved = member(88);
    saved.rescued = true;
    saved.modifiers = {50'000, 50'000, 50'000, 50'000, 50'000, 50'000};
    auto ledger = ledger_with_member(saved);
    FateExecutionCounters counters;
    const FateStageOne quota{crisis(4, 1'000), 100, 10, 90};
    const auto result = FateResolver::resolve_present(ledger, quota, FateRules{}, counters);
    EXPECT_NE(result.decisions.front().outcome, FateOutcome::Died);
    EXPECT_EQ(result.named_deaths, 0U);
    EXPECT_EQ(result.unnamed_deaths, 10U);
}

}  // namespace
