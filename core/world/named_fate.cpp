// named_fate.cpp：以權威統計配額分配具名命運，並共用在場與重載補算實作。

#include "core/world/named_fate.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>
#include <tuple>
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

[[nodiscard]] std::uint64_t splitmix64(std::uint64_t value) noexcept {
    value += UINT64_C(0x9E3779B97F4A7C15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31U);
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

std::uint64_t fate_seed(std::uint64_t event_id, std::uint64_t cohort_id,
                        std::uint64_t entity_uid) noexcept {
    auto seed = splitmix64(event_id);
    seed = splitmix64(seed ^ cohort_id);
    return splitmix64(seed ^ entity_uid);
}

std::uint32_t adjusted_death_basis_points(const FateStageOne& stage_one,
                                          const NamedFateMember& member,
                                          const FateRules& rules) {
    if (!valid_stage_one(stage_one) || !valid_member(member) ||
        member.cohort_id != stage_one.crisis.cohort_id ||
        rules.marked_death_bias_basis_points == 0 ||
        rules.marked_death_bias_basis_points > kFateBasisPoints) {
        throw std::invalid_argument{"個體命運修正輸入無效"};
    }
    const auto weight_sum = std::accumulate(rules.surviving_outcome_weights.begin(),
                                            rules.surviving_outcome_weights.end(), UINT64_C(0));
    if (weight_sum == 0) {
        throw std::invalid_argument{"存活命運權重總和不可為零"};
    }
    const auto cohort_loss_basis_points =
        stage_one.population_before == 0
            ? UINT64_C(0)
            : static_cast<std::uint64_t>(stage_one.total_loss) * kFateBasisPoints /
                  stage_one.population_before;
    std::uint64_t adjusted = cohort_loss_basis_points;
    const std::array modifiers{
        member.modifiers.status_basis_points,
        member.modifiers.wealth_basis_points,
        member.modifiers.relationship_basis_points,
        member.modifiers.occupation_basis_points,
        member.modifiers.vulnerability_basis_points,
        member.modifiers.district_basis_points,
    };
    for (const auto modifier : modifiers) {
        adjusted = std::min<std::uint64_t>(
            kFateBasisPoints, adjusted * modifier / kFateBasisPoints);
    }
    if (member.marked) {
        adjusted = adjusted * rules.marked_death_bias_basis_points / kFateBasisPoints;
        adjusted = std::max<std::uint64_t>(1U, adjusted);
    }
    return static_cast<std::uint32_t>(adjusted);
}

FateOutcome roll_fate_outcome(std::mt19937_64& rng, std::uint32_t death_basis_points,
                              const FateRules& rules) {
    if (death_basis_points > kFateBasisPoints) {
        throw std::invalid_argument{"死亡機率超過 10000 basis points"};
    }
    const auto death_roll = static_cast<std::uint32_t>(rng() % kFateBasisPoints);
    if (death_roll < death_basis_points) {
        return FateOutcome::Died;
    }
    const auto total = std::accumulate(rules.surviving_outcome_weights.begin(),
                                       rules.surviving_outcome_weights.end(), UINT64_C(0));
    if (total == 0) {
        throw std::invalid_argument{"存活命運權重總和不可為零"};
    }
    auto roll = rng() % total;
    for (std::size_t index = 0; index < rules.surviving_outcome_weights.size(); ++index) {
        const auto weight = rules.surviving_outcome_weights[index];
        if (roll < weight) {
            return static_cast<FateOutcome>(index);
        }
        roll -= weight;
    }
    throw std::logic_error{"存活命運抽樣未命中任何權重"};
}

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
