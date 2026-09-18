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

[[nodiscard]] std::size_t outcome_index(FateOutcome outcome) {
    return static_cast<std::size_t>(outcome);
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
