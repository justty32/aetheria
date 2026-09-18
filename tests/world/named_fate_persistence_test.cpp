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

}  // namespace
