#include "core/serialize/normalized_state_hash.h"

#include <aetheria/runtime/cross_zone.h>

#include "core/site/site_build_loop.h"
#include "core/site/site_lifecycle.h"
#include "core/local/local_navigation.h"
#include "core/world/army_state.h"
#include "core/world/region_movement.h"
#include "core/world/named_fate.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/serialize/normalized_state_hash_detail.h"

namespace aetheria::serialize::detail {

namespace {

void hash_fate_stage_one(std::uint64_t& hash, const world::FateStageOne& stage) noexcept {
    hash_scalar(hash, stage.crisis.event_id);
    hash_scalar(hash, stage.crisis.cohort_id);
    hash_scalar(hash, stage.crisis.site_key);
    hash_scalar(hash, stage.crisis.base_loss_basis_points);
    hash_scalar(hash, stage.crisis.relief_basis_points);
    hash_scalar(hash, static_cast<std::int64_t>(stage.crisis.occurred_at));
    hash_string(hash, stage.crisis.place_key);
    hash_scalar(hash, stage.population_before);
    hash_scalar(hash, stage.total_loss);
    hash_scalar(hash, stage.population_after);
}

} // namespace

void hash_named_fate_ledger(std::uint64_t& hash, const zone::Zone& zone) {
    const auto ledgers = zone.reg.view<const world::NamedFateLedger>();
    if (ledgers.size() > 1U) {
        throw std::runtime_error{"正規化雜湊遇到多個 NamedFateLedger"};
    }
    hash_scalar(hash, static_cast<std::uint64_t>(ledgers.size()));
    if (ledgers.empty()) {
        return;
    }
    const auto& ledger = ledgers.get<const world::NamedFateLedger>(*ledgers.begin());
    const auto level = zone::level_of(zone.key);
    if ((level != zone::ZoneLevel::Region && level != zone::ZoneLevel::Site) ||
        !world::valid_named_fate_ledger(ledger)) {
        throw std::runtime_error{"正規化雜湊遇到無效 NamedFateLedger"};
    }
    auto members = ledger.members;
    std::ranges::sort(members, {}, &world::NamedFateMember::entity_uid);
    hash_scalar(hash, static_cast<std::uint64_t>(members.size()));
    for (const auto& member : members) {
        hash_scalar(hash, member.entity_uid);
        hash_scalar(hash, member.cohort_id);
        hash_string(hash, member.name_key);
        hash_scalar(hash, member.significance);
        hash_string(hash, member.significance_reason);
        hash_scalar(hash, member.modifiers.status_basis_points);
        hash_scalar(hash, member.modifiers.wealth_basis_points);
        hash_scalar(hash, member.modifiers.relationship_basis_points);
        hash_scalar(hash, member.modifiers.occupation_basis_points);
        hash_scalar(hash, member.modifiers.vulnerability_basis_points);
        hash_scalar(hash, member.modifiers.district_basis_points);
        hash_scalar(hash, static_cast<std::uint8_t>(member.marked));
        hash_scalar(hash, static_cast<std::uint8_t>(member.rescued));
        hash_scalar(hash, static_cast<std::uint8_t>(member.has_outcome));
        hash_scalar(hash, member.outcome);
    }
    auto pending = ledger.pending;
    std::ranges::sort(pending, [](const auto& left, const auto& right) {
        return std::tie(left.crisis.site_key, left.crisis.event_id, left.crisis.cohort_id) <
               std::tie(right.crisis.site_key, right.crisis.event_id, right.crisis.cohort_id);
    });
    hash_scalar(hash, static_cast<std::uint64_t>(pending.size()));
    for (const auto& stage : pending) {
        hash_fate_stage_one(hash, stage);
    }
    hash_scalar(hash, static_cast<std::uint64_t>(ledger.events.size()));
    for (const auto& event : ledger.events) {
        hash_scalar(hash, event.kind);
        hash_scalar(hash, event.event_id);
        hash_scalar(hash, event.cohort_id);
        hash_scalar(hash, event.entity_uid);
        hash_string(hash, event.place_key);
        hash_string(hash, event.person_key);
        hash_string(hash, event.template_key);
        hash_scalar(hash, event.outcome);
    }
}

} // namespace aetheria::serialize::detail
