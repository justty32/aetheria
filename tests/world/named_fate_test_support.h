#pragma once

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

namespace aetheria::tests::named_fate {

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

inline constexpr std::uint64_t kCohortId = 77;

inline constexpr std::uint64_t kSiteKeyValue = UINT64_C(0x2000000000010001);

[[nodiscard]] inline aetheria::zone::Zone make_site_zone(std::uint32_t population) {
    const auto region = aetheria::zone::child_key(aetheria::zone::kRootZone, 1, 0);
    aetheria::zone::Zone site{aetheria::zone::child_key(region, 0, 0)};
    const auto placeholder = *site.reg.view<aetheria::zone::ZoneMeta>().begin();
    aetheria::site::CityBuildState state;
    state.economy.population = population;
    site.reg.emplace<aetheria::site::CityBuildState>(placeholder, std::move(state));
    return site;
}

[[nodiscard]] inline FateCrisis crisis(std::uint64_t event_id, std::uint32_t loss_basis_points = 1'200,
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

[[nodiscard]] inline FateStageOne stage(std::uint64_t event_id,
                                 std::uint32_t loss_basis_points = 1'200) {
    const std::uint32_t before = 10'000;
    const auto loss = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(before) * loss_basis_points /
        aetheria::world::kFateBasisPoints);
    return {crisis(event_id, loss_basis_points), before, loss,
            static_cast<std::uint32_t>(before - loss)};
}

[[nodiscard]] inline NamedFateMember member(std::uint64_t uid, bool marked = false) {
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

[[nodiscard]] inline NamedFateLedger ledger_with_member(NamedFateMember named) {
    NamedFateLedger ledger;
    ledger.members.push_back(std::move(named));
    return ledger;
}

}  // namespace aetheria::tests::named_fate
