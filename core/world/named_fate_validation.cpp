// 具名命運成員、階段結果與持久 ledger 的有效性驗證。

#include "core/world/named_fate.h"
#include "core/world/named_fate_detail.h"

#include <set>
#include <utility>

namespace aetheria::world {
namespace {

inline constexpr std::uint32_t kMaximumModifierBasisPoints = 50'000;

[[nodiscard]] bool valid_outcome(FateOutcome outcome) noexcept {
    return outcome <= FateOutcome::Died;
}

[[nodiscard]] bool valid_modifier(std::uint32_t value) noexcept {
    return value > 0 && value <= kMaximumModifierBasisPoints;
}

}  // namespace

namespace detail {

[[nodiscard]] bool valid_member(const NamedFateMember& member) noexcept {
    return member.entity_uid != 0 && member.cohort_id != 0 && !member.name_key.empty() &&
           member.significance <= Significance::World &&
           (!member.has_outcome || valid_outcome(member.outcome)) &&
           valid_modifier(member.modifiers.status_basis_points) &&
           valid_modifier(member.modifiers.wealth_basis_points) &&
           valid_modifier(member.modifiers.relationship_basis_points) &&
           valid_modifier(member.modifiers.occupation_basis_points) &&
           valid_modifier(member.modifiers.vulnerability_basis_points) &&
           valid_modifier(member.modifiers.district_basis_points);
}

[[nodiscard]] bool valid_stage_one(const FateStageOne& stage) noexcept {
    return stage.crisis.event_id != 0 && stage.crisis.cohort_id != 0 &&
           stage.crisis.site_key != 0 && !stage.crisis.place_key.empty() &&
           stage.crisis.base_loss_basis_points <= kFateBasisPoints &&
           stage.crisis.relief_basis_points <= kFateBasisPoints &&
           time::is_representable(stage.crisis.occurred_at) &&
           stage.population_after <= stage.population_before &&
           stage.total_loss == stage.population_before - stage.population_after;
}

}  // namespace detail

using detail::valid_member;
using detail::valid_stage_one;

bool valid_named_fate_ledger(const NamedFateLedger& ledger) noexcept {
    std::set<std::uint64_t> member_uids;
    for (const auto& member : ledger.members) {
        if (!valid_member(member) || !member_uids.insert(member.entity_uid).second) {
            return false;
        }
    }
    std::set<std::pair<std::uint64_t, std::uint64_t>> pending_ids;
    for (const auto& pending : ledger.pending) {
        if (!valid_stage_one(pending) ||
            !pending_ids.emplace(pending.crisis.event_id, pending.crisis.cohort_id).second) {
            return false;
        }
    }
    for (const auto& event : ledger.events) {
        if (event.event_id == 0 || event.cohort_id == 0 || event.place_key.empty() ||
            event.template_key.empty() || event.kind > FateNarrativeKind::NamedQuotaOverflow ||
            !valid_outcome(event.outcome)) {
            return false;
        }
        if (event.kind == FateNarrativeKind::NamedOutcome &&
            (event.entity_uid == 0 || event.person_key.empty())) {
            return false;
        }
    }
    return true;
}

}  // namespace aetheria::world
