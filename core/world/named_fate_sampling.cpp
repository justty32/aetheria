// 具名命運的個體 seed、死亡機率修正與結果抽樣。

#include "core/world/named_fate.h"
#include "core/world/named_fate_detail.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace aetheria::world {
using detail::valid_member;
using detail::valid_stage_one;
namespace {

[[nodiscard]] std::uint64_t splitmix64(std::uint64_t value) noexcept {
    value += UINT64_C(0x9E3779B97F4A7C15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31U);
}

}  // namespace

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

}  // namespace aetheria::world
