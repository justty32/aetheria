// 外交勢力國力真值與 AI 心智狀態存取。

#include "core/world/diplomacy.h"

#include <limits>
#include <stdexcept>

namespace aetheria::world {

void WorldDiplomacyState::set_faction_truth(FactionId faction,
                                            std::int32_t military_power,
                                            std::int32_t economic_power) {
    if (military_power < 0 || economic_power < 0) {
        throw std::invalid_argument{"勢力國力真值不可為負"};
    }
    if (!truths_.has_value()) {
        truths_.emplace(static_cast<std::size_t>(faction_count_) + 1U);
    }
    truths_->at(faction_index(faction)) = {military_power, economic_power, true};
}

void WorldDiplomacyState::adjust_faction_truth(FactionId faction,
                                               std::int32_t military_delta,
                                               std::int32_t economic_delta) {
    const auto current = faction_truth(faction);
    if (!current.has_value()) {
        throw std::logic_error{"缺席的勢力真值不可直接調整"};
    }
    const auto military = static_cast<std::int64_t>(current->military_power) + military_delta;
    const auto economic = static_cast<std::int64_t>(current->economic_power) + economic_delta;
    if (military < 0 || economic < 0 || military > std::numeric_limits<std::int32_t>::max() ||
        economic > std::numeric_limits<std::int32_t>::max()) {
        throw std::overflow_error{"勢力國力調整超出可表達範圍"};
    }
    set_faction_truth(faction, static_cast<std::int32_t>(military),
                      static_cast<std::int32_t>(economic));
}

std::optional<FactionTruth> WorldDiplomacyState::faction_truth(FactionId faction) const {
    if (!truths_.has_value()) {
        return std::nullopt;
    }
    const auto& truth = truths_->at(faction_index(faction));
    return truth.present ? std::optional<FactionTruth>{truth} : std::nullopt;
}

ai::FactionMindState& WorldDiplomacyState::faction_mind(FactionId faction) {
    if (!faction_minds_.has_value()) {
        faction_minds_.emplace(static_cast<std::size_t>(faction_count_) + 1U);
    }
    return faction_minds_->at(faction_index(faction));
}

const ai::FactionMindState& WorldDiplomacyState::faction_mind(FactionId faction) const {
    if (!faction_minds_.has_value()) {
        throw std::logic_error{"AI 目標狀態在舊版存檔中缺席"};
    }
    return faction_minds_->at(faction_index(faction));
}

} // namespace aetheria::world
