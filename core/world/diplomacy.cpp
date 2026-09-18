// 外交世界狀態的有向關係演化、期限檢查與持續戰爭推進。
// 關係矩陣只寫指定方向；所有可調速率與門檻均來自 Ruleset。

#include "core/world/diplomacy.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace aetheria::world {
namespace {

[[nodiscard]] constexpr std::size_t faction_value(FactionId faction) noexcept {
    return static_cast<std::size_t>(faction);
}

[[nodiscard]] std::int32_t
revert_toward_zero(std::int32_t value, std::uint16_t rate,
                   std::uint16_t denominator) noexcept {
    if (value == 0) {
        return 0;
    }
    const auto magnitude = std::abs(static_cast<std::int64_t>(value));
    const auto reduction = std::max<std::int64_t>(
        1, (magnitude * rate + denominator - 1) / denominator);
    return static_cast<std::int32_t>(value > 0 ? magnitude - reduction
                                               : reduction - magnitude);
}

} // namespace

WorldDiplomacyState::WorldDiplomacyState(std::uint16_t faction_count,
                                         std::uint64_t world_seed,
                                         const rules::Ruleset& ruleset)
    : faction_count_{faction_count}, world_seed_{world_seed},
      ruleset_{&ruleset} {
    if (faction_count == 0 || !ruleset.diplomacy_rules().loaded) {
        throw std::invalid_argument{
            "外交狀態需要至少一個勢力與已載入的外交規則"};
    }
    const auto extent = static_cast<std::size_t>(faction_count_) + 1U;
    relations_.resize(extent * extent);
    truths_.emplace(extent);
    knowledge_.emplace(extent * extent);
    faction_minds_.emplace(extent);
}

std::size_t WorldDiplomacyState::faction_index(FactionId faction) const {
    const auto index = faction_value(faction);
    if (index == 0 || index > faction_count_) {
        throw std::out_of_range{"外交勢力 id 超出範圍"};
    }
    return index;
}

std::size_t WorldDiplomacyState::matrix_index(FactionId observer,
                                              FactionId target) const {
    const auto extent = static_cast<std::size_t>(faction_count_) + 1U;
    return faction_index(observer) * extent + faction_index(target);
}

const DiplomaticRelation&
WorldDiplomacyState::relation(FactionId observer, FactionId target) const {
    return relations_.at(matrix_index(observer, target));
}

std::int32_t
WorldDiplomacyState::bounded_relation(std::int64_t value) const noexcept {
    const auto& rules = ruleset_->diplomacy_rules();
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        value, rules.relation_min, rules.relation_max));
}

void WorldDiplomacyState::set_relation(FactionId observer, FactionId target,
                                       DiplomaticRelation value) {
    const auto bounded = DiplomaticRelation{
        .favor = bounded_relation(value.favor),
        .trust = bounded_relation(value.trust),
        .fear = bounded_relation(value.fear),
        .grievance = bounded_relation(value.grievance),
    };
    relations_.at(matrix_index(observer, target)) = bounded;
}

void WorldDiplomacyState::adjust_relation(FactionId observer, FactionId target,
                                          DiplomaticRelation delta) {
    auto& value = relations_.at(matrix_index(observer, target));
    value.favor =
        bounded_relation(static_cast<std::int64_t>(value.favor) + delta.favor);
    value.trust =
        bounded_relation(static_cast<std::int64_t>(value.trust) + delta.trust);
    value.fear =
        bounded_relation(static_cast<std::int64_t>(value.fear) + delta.fear);
    value.grievance = bounded_relation(
        static_cast<std::int64_t>(value.grievance) + delta.grievance);
}

void WorldDiplomacyState::advance_relations_xun() {
    const auto& rates = ruleset_->diplomacy_rules().reversion;
    for (std::size_t observer = 1; observer <= faction_count_; ++observer) {
        for (std::size_t target = 1; target <= faction_count_; ++target) {
            auto& value =
                relations_[observer *
                               (static_cast<std::size_t>(faction_count_) + 1U) +
                           target];
            value.favor =
                revert_toward_zero(value.favor, rates.favor, rates.denominator);
            value.trust =
                revert_toward_zero(value.trust, rates.trust, rates.denominator);
            value.fear =
                revert_toward_zero(value.fear, rates.fear, rates.denominator);
            value.grievance = revert_toward_zero(
                value.grievance, rates.grievance, rates.denominator);
        }
    }
}

} // namespace aetheria::world
