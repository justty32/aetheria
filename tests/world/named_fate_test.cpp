#include "core/rules/attributes.h"
#include "core/serialize/zone_codec.h"
#include "core/serialize/normalized_state_hash.h"
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

constexpr std::uint64_t kCohortId = 77;
constexpr std::uint64_t kSiteKeyValue = UINT64_C(0x2000000000010001);
constexpr RegionXY kCoordinate{};

[[nodiscard]] aetheria::zone::Zone make_site_zone(std::uint32_t population) {
    const auto region = aetheria::zone::child_key(aetheria::zone::kRootZone, 1, 0);
    aetheria::zone::Zone site{aetheria::zone::child_key(region, 0, 0)};
    const auto placeholder = *site.reg.view<aetheria::zone::ZoneMeta>().begin();
    aetheria::site::CityBuildState state;
    state.economy.population = population;
    site.reg.emplace<aetheria::site::CityBuildState>(placeholder, std::move(state));
    return site;
}

[[nodiscard]] RegionTiles region_with_population(std::uint32_t population) {
    RegionTiles tiles{1, 1};
    auto site = make_site_zone(population);
    aetheria::site::ReductionTable::apply(
        tiles, kCoordinate, aetheria::site::ReductionTable::reduce(site));
    return tiles;
}

[[nodiscard]] FateCrisis crisis(std::uint64_t event_id, std::uint32_t loss_basis_points = 1'200,
                                std::uint32_t relief_basis_points = 0) {
    return {
        .event_id = event_id,
        .cohort_id = kCohortId,
        .site_key = kSiteKeyValue,
        .base_loss_basis_points = loss_basis_points,
        .relief_basis_points = relief_basis_points,
        .occurred_at = aetheria::time::Tick{123},
        .place_key = "place.stonebridge",
    };
}

[[nodiscard]] FateStageOne stage(std::uint64_t event_id,
                                 std::uint32_t loss_basis_points = 1'200) {
    const std::uint32_t before = 10'000;
    const auto loss = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(before) * loss_basis_points /
        aetheria::world::kFateBasisPoints);
    return {crisis(event_id, loss_basis_points), before, loss,
            static_cast<std::uint32_t>(before - loss)};
}

[[nodiscard]] NamedFateMember member(std::uint64_t uid, bool marked = false) {
    return {
        .entity_uid = uid,
        .cohort_id = kCohortId,
        .name_key = "person.named." + std::to_string(uid),
        .significance = aetheria::world::Significance::Local,
        .significance_reason = "玩家認識的具名市民",
        .modifiers = {},
        .marked = marked,
        .rescued = false,
        .has_outcome = false,
        .outcome = FateOutcome::Unharmed,
    };
}

[[nodiscard]] NamedFateLedger ledger_with_member(NamedFateMember named) {
    NamedFateLedger ledger;
    ledger.members.push_back(std::move(named));
    return ledger;
}

[[nodiscard]] std::size_t outcome_index(FateOutcome outcome) {
    return static_cast<std::size_t>(outcome);
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

TEST(NamedFate, MarkBiasIsNotImmunityAndAllFiveOutcomesAreHitInOneFixedRun) {
    constexpr std::size_t samples = 10'000;
    auto unmarked = ledger_with_member(member(9001, false));
    auto marked = ledger_with_member(member(9001, true));
    FateExecutionCounters counters;
    std::array<std::uint32_t, 5> unmarked_hits{};
    std::array<std::uint32_t, 5> marked_hits{};
    for (std::uint64_t sample = 1; sample <= samples; ++sample) {
        const auto current = stage(UINT64_C(0xF0000000) + sample);
        const auto ordinary =
            FateResolver::resolve_present(unmarked, current, FateRules{}, counters);
        const auto favored = FateResolver::resolve_present(marked, current, FateRules{}, counters);
        ++unmarked_hits[outcome_index(ordinary.decisions.front().outcome)];
        ++marked_hits[outcome_index(favored.decisions.front().outcome)];
    }
    EXPECT_GT(marked_hits[outcome_index(FateOutcome::Died)], 0U);
    EXPECT_LT(marked_hits[outcome_index(FateOutcome::Died)],
              unmarked_hits[outcome_index(FateOutcome::Died)]);
    for (const auto hits : unmarked_hits) {
        EXPECT_GT(hits, 0U);
    }
    std::cout << "named_fate_bias samples=" << samples
              << " unmarked_deaths=" << unmarked_hits[4]
              << " marked_deaths=" << marked_hits[4]
              << " outcomes_unharmed_injured_property_displaced_died=" << unmarked_hits[0] << ','
              << unmarked_hits[1] << ',' << unmarked_hits[2] << ',' << unmarked_hits[3] << ','
              << unmarked_hits[4] << '\n';
}

TEST(NamedFate, PresentAndReloadCatchUpExecuteTheSameDistribution) {
    constexpr std::uint64_t samples = 2'000;
    auto present = ledger_with_member(member(404, true));
    auto reloaded = ledger_with_member(member(404, true));
    NamedFateLedger region_queue;
    FateExecutionCounters counters;
    std::uint32_t present_deaths{};
    for (std::uint64_t sample = 1; sample <= samples; ++sample) {
        const auto current = stage(UINT64_C(0xAB000000) + sample);
        const auto result = FateResolver::resolve_present(present, current, FateRules{}, counters);
        present_deaths += result.named_deaths;
        FateResolver::enqueue_absent(region_queue, current);
    }
    const auto results = FateResolver::resolve_reload(reloaded, region_queue, kSiteKeyValue,
                                                      FateRules{}, counters);
    std::uint32_t reload_deaths{};
    for (const auto& result : results) {
        reload_deaths += result.named_deaths;
    }
    const auto relative_error = std::abs(static_cast<double>(present_deaths) - reload_deaths) /
                                std::max(1.0, static_cast<double>(present_deaths));
    EXPECT_EQ(results.size(), samples);
    EXPECT_EQ(counters.present, samples);
    EXPECT_EQ(counters.reload_catch_up, samples);
    EXPECT_TRUE(region_queue.pending.empty());
    EXPECT_LT(relative_error, 0.05);
    std::cout << "named_fate_unbiased present_executions=" << counters.present
              << " reload_executions=" << counters.reload_catch_up
              << " present_deaths=" << present_deaths
              << " reload_deaths=" << reload_deaths
              << " relative_error=" << relative_error << '\n';
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

TEST(NamedFate, UidOrderingMakesInsertionOrderAndPersistenceDeterministic) {
    NamedFateLedger ascending;
    NamedFateLedger descending;
    for (std::uint64_t uid = 1; uid <= 20; ++uid) {
        ascending.members.push_back(member(uid));
        descending.members.push_back(member(21 - uid));
    }
    FateExecutionCounters counters;
    const auto current = stage(5);
    const auto first = FateResolver::resolve_present(ascending, current, FateRules{}, counters);
    const auto second = FateResolver::resolve_present(descending, current, FateRules{}, counters);
    EXPECT_EQ(first.decisions, second.decisions);
    EXPECT_EQ(ascending.events, descending.events);

    auto site = make_site_zone(10'000);
    const auto placeholder = *site.reg.view<aetheria::zone::ZoneMeta>().begin();
    site.reg.emplace<NamedFateLedger>(placeholder, ascending);
    const auto fate_hash =
        aetheria::serialize::normalized_state_hash(site, aetheria::tests::test_ruleset());
    const auto bytes = aetheria::serialize::encode_zone(site, aetheria::tests::test_ruleset());
    site.reg.clear();
    const auto loaded =
        aetheria::serialize::decode_zone(bytes, aetheria::tests::test_ruleset());
    const auto ledgers = loaded->reg.view<const NamedFateLedger>();
    ASSERT_EQ(ledgers.size(), 1U);
    EXPECT_EQ(ledgers.get<const NamedFateLedger>(*ledgers.begin()), ascending);
    EXPECT_EQ(aetheria::serialize::normalized_state_hash(*loaded,
                                                        aetheria::tests::test_ruleset()),
              fate_hash);
    EXPECT_EQ(aetheria::serialize::encode_zone(*loaded, aetheria::tests::test_ruleset()), bytes);
    std::cout << "named_fate_determinism members=" << first.decisions.size()
              << " canonical_first_uid=" << first.decisions.front().entity_uid
              << " persisted_events=" << ascending.events.size()
              << " bytes=" << bytes.size() << " normalized_hash=" << fate_hash << '\n';
}

TEST(NamedFate, NamedTierOverrideHasARealOneTierDifferenceAndReason) {
    const auto& rules = aetheria::tests::test_ruleset().attribute_rules();
    const aetheria::rules::Attributes attributes{.body = 10, .skill = 10, .mind = 10, .spirit = 10};
    const auto suggested = aetheria::rules::suggest_tier(attributes, rules);
    const auto actual = aetheria::rules::resolve_tier(
        attributes, rules,
        aetheria::rules::TierOverride{.tier = aetheria::world::Significance::Local,
                                      .reason = "玩家反覆互動後成為具名人物"});
    auto named = member(777);
    named.significance = actual;
    named.significance_reason = "玩家反覆互動後成為具名人物";
    auto ledger = ledger_with_member(named);
    EXPECT_TRUE(aetheria::world::valid_named_fate_ledger(ledger));
    EXPECT_EQ(suggested, aetheria::world::Significance::Ambient);
    EXPECT_EQ(actual, aetheria::world::Significance::Local);
    EXPECT_EQ(static_cast<int>(actual) - static_cast<int>(suggested), 1);
}

}  // namespace
