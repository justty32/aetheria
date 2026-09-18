// core/rules/combat_loss_allocation.cpp：依位階門檻與最大餘數分配參戰者損失。

#include "core/rules/combat.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include "core/base/check.h"

namespace aetheria::rules {

std::vector<ParticipantFate> allocate_combat_loss(
    std::int32_t total_loss, std::span<const LossParticipant> participants,
    world::Significance attacker_tier, const PowerRules& power_rules,
    const PowerBreakthroughDef* breakthrough) noexcept {
    AETH_CHECK(total_loss >= 0);
    std::vector<ParticipantFate> result;
    result.reserve(participants.size());
    std::vector<std::int32_t> weights;
    weights.reserve(participants.size());
    std::int64_t total_weight{};
    for (const auto& participant : participants) {
        AETH_CHECK(participant.capacity >= 0);
        const auto weight = static_cast<std::int32_t>(apply_individual_tier_gate(
            participant.capacity, attacker_tier, participant.tier, participant.individual,
            power_rules, breakthrough));
        weights.push_back(weight);
        total_weight += weight;
        result.push_back({participant.participant_id, 0});
    }
    AETH_CHECK(total_loss == 0 || total_weight >= total_loss);
    if (total_loss == 0) {
        return result;
    }

    struct Remainder {
        std::int64_t value{};
        std::size_t index{};
    };
    std::vector<Remainder> remainders;
    remainders.reserve(participants.size());
    std::int32_t allocated{};
    for (std::size_t index = 0; index < participants.size(); ++index) {
        const auto product = static_cast<std::int64_t>(total_loss) * weights[index];
        result[index].allocated_loss = static_cast<std::int32_t>(product / total_weight);
        allocated += result[index].allocated_loss;
        remainders.push_back({product % total_weight, index});
    }
    std::ranges::sort(remainders, [](const Remainder& left, const Remainder& right) {
        if (left.value != right.value) {
            return left.value > right.value;
        }
        return left.index < right.index;
    });
    const auto missing = total_loss - allocated;
    AETH_CHECK(missing >= 0);
    AETH_CHECK(static_cast<std::size_t>(missing) <= remainders.size());
    for (std::int32_t index = 0; index < missing; ++index) {
        ++result[remainders[static_cast<std::size_t>(index)].index].allocated_loss;
    }
    AETH_CHECK(std::accumulate(result.begin(), result.end(), std::int64_t{},
                               [](std::int64_t sum, const ParticipantFate& fate) {
                                   return sum + fate.allocated_loss;
                               }) == total_loss);
    return result;
}

}  // namespace aetheria::rules
