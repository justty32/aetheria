// named_fate.cpp：以權威統計配額分配具名命運，並共用在場與重載補算實作。

#include "core/world/named_fate.h"
#include "core/world/named_fate_detail.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace aetheria::world {
using detail::valid_stage_one;
namespace {

[[nodiscard]] std::string template_key(FateOutcome outcome) {
    switch (outcome) {
    case FateOutcome::Unharmed:
        return "event.fate.named.unharmed";
    case FateOutcome::Injured:
        return "event.fate.named.injured";
    case FateOutcome::PropertyLost:
        return "event.fate.named.property_lost";
    case FateOutcome::Displaced:
        return "event.fate.named.displaced";
    case FateOutcome::Died:
        return "event.fate.named.died";
    }
    throw std::logic_error{"命運結果列舉無效"};
}

[[nodiscard]] FateResolution resolve(NamedFateLedger& ledger, const FateStageOne& stage_one,
                                     const FateRules& rules, FateResolutionPath path) {
    if (!valid_stage_one(stage_one) || !valid_named_fate_ledger(ledger)) {
        throw std::invalid_argument{"命運判定收到無效持久狀態或階段 1 結果"};
    }
    FateResolution result{
        .path = path,
        .stage_one_total_loss = stage_one.total_loss,
        .named_deaths = 0,
        .unnamed_deaths = 0,
        .named_quota_overflow = false,
        .decisions = {},
    };
    std::vector<NamedFateMember*> members;
    for (auto& member : ledger.members) {
        if (member.cohort_id == stage_one.crisis.cohort_id) {
            members.push_back(&member);
        }
    }
    std::ranges::sort(members, {}, &NamedFateMember::entity_uid);
    result.decisions.reserve(members.size());
    for (auto* member : members) {
        const auto death_basis_points =
            adjusted_death_basis_points(stage_one, *member, rules);
        std::mt19937_64 rng{
            fate_seed(stage_one.crisis.event_id, stage_one.crisis.cohort_id,
                      member->entity_uid)};
        const auto outcome = member->rescued
                                 ? roll_fate_outcome(rng, 0, rules)
                                 : roll_fate_outcome(rng, death_basis_points, rules);
        member->has_outcome = true;
        member->outcome = outcome;
        result.named_deaths += outcome == FateOutcome::Died ? 1U : 0U;
        result.decisions.push_back({member->entity_uid, death_basis_points, outcome});
        ledger.events.push_back({
            .kind = FateNarrativeKind::NamedOutcome,
            .event_id = stage_one.crisis.event_id,
            .cohort_id = stage_one.crisis.cohort_id,
            .entity_uid = member->entity_uid,
            .place_key = stage_one.crisis.place_key,
            .person_key = member->name_key,
            .template_key = template_key(outcome),
            .outcome = outcome,
        });
    }
    result.unnamed_deaths = result.named_deaths >= stage_one.total_loss
                                ? 0U
                                : stage_one.total_loss - result.named_deaths;
    result.named_quota_overflow = result.named_deaths > stage_one.total_loss;
    if (result.named_quota_overflow) {
        ledger.events.push_back({
            .kind = FateNarrativeKind::NamedQuotaOverflow,
            .event_id = stage_one.crisis.event_id,
            .cohort_id = stage_one.crisis.cohort_id,
            .entity_uid = 0,
            .place_key = stage_one.crisis.place_key,
            .person_key = {},
            .template_key = "event.fate.named_quota_overflow",
            .outcome = FateOutcome::Unharmed,
        });
    }
    return result;
}

}  // namespace

FateStageOne FateResolver::apply_stage_one(RegionTiles& tiles, RegionXY coordinate,
                                           FateCrisis crisis) {
    if (!tiles.valid_layout() || crisis.event_id == 0 || crisis.cohort_id == 0 ||
        crisis.site_key == 0 || crisis.place_key.empty() ||
        crisis.base_loss_basis_points > kFateBasisPoints ||
        crisis.relief_basis_points > kFateBasisPoints ||
        !time::is_representable(crisis.occurred_at)) {
        throw std::invalid_argument{"階段 1 災難輸入無效"};
    }
    const auto index = tiles.index_of(coordinate);
    auto& populations =
        std::get<ReductionField<PopulationReduction>>(tiles.reduction_fields_.fields).values;
    const auto before = populations[index];
    const auto numerator = static_cast<std::uint64_t>(before) *
                           crisis.base_loss_basis_points *
                           (kFateBasisPoints - crisis.relief_basis_points);
    const auto loss = static_cast<std::uint32_t>(
        numerator / (static_cast<std::uint64_t>(kFateBasisPoints) * kFateBasisPoints));
    populations[index] = before - loss;
    return {std::move(crisis), before, loss, static_cast<std::uint32_t>(before - loss)};
}

FateResolution FateResolver::resolve_present(NamedFateLedger& site_ledger,
                                             const FateStageOne& stage_one,
                                             const FateRules& rules,
                                             FateExecutionCounters& counters) {
    auto result = resolve(site_ledger, stage_one, rules, FateResolutionPath::Present);
    ++counters.present;
    return result;
}

void FateResolver::enqueue_absent(NamedFateLedger& region_ledger, FateStageOne stage_one) {
    if (!valid_stage_one(stage_one) || !valid_named_fate_ledger(region_ledger)) {
        throw std::invalid_argument{"離線命運排程收到無效資料"};
    }
    if (std::ranges::any_of(region_ledger.pending, [&](const auto& pending) {
            return pending.crisis.event_id == stage_one.crisis.event_id &&
                   pending.crisis.cohort_id == stage_one.crisis.cohort_id;
        })) {
        throw std::logic_error{"同一 cohort 的同一命運事件不可重複排程"};
    }
    region_ledger.pending.push_back(std::move(stage_one));
    std::ranges::sort(region_ledger.pending, [](const auto& left, const auto& right) {
        return std::tie(left.crisis.site_key, left.crisis.event_id, left.crisis.cohort_id) <
               std::tie(right.crisis.site_key, right.crisis.event_id, right.crisis.cohort_id);
    });
}

std::vector<FateResolution> FateResolver::resolve_reload(
    NamedFateLedger& site_ledger, NamedFateLedger& region_ledger, std::uint64_t site_key,
    const FateRules& rules, FateExecutionCounters& counters) {
    if (site_key == 0 || !valid_named_fate_ledger(site_ledger) ||
        !valid_named_fate_ledger(region_ledger)) {
        throw std::invalid_argument{"重載命運補算收到無效 ledger"};
    }
    std::vector<FateResolution> results;
    for (auto iterator = region_ledger.pending.begin(); iterator != region_ledger.pending.end();) {
        if (iterator->crisis.site_key != site_key) {
            ++iterator;
            continue;
        }
        results.push_back(resolve(site_ledger, *iterator, rules,
                                  FateResolutionPath::ReloadCatchUp));
        ++counters.reload_catch_up;
        iterator = region_ledger.pending.erase(iterator);
    }
    return results;
}

}  // namespace aetheria::world
