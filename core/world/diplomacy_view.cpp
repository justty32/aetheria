// 世界外交真值依 seed 產生估計，並複本化為 AI 可見的 FactionView。
// 本檔是唯一把 WorldDiplomacyState 私有情報轉成公開知識快照的入口。

#include "core/world/diplomacy.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace aetheria::world {
namespace {

[[nodiscard]] constexpr std::size_t faction_value(FactionId faction) noexcept {
    return static_cast<std::size_t>(faction);
}

[[nodiscard]] constexpr std::uint64_t mix64(std::uint64_t value) noexcept {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
}

[[nodiscard]] std::int32_t saturating_int32(std::int64_t value) noexcept {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        value, std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()));
}

} // namespace

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

std::int32_t WorldDiplomacyState::estimated_power(
    FactionId observer, FactionId target, std::int32_t truth,
    std::uint16_t uncertainty, time::Tick observed_at,
    std::uint64_t channel) const noexcept {
    const auto key =
        world_seed_ ^
        (static_cast<std::uint64_t>(faction_value(observer)) << 48U) ^
        (static_cast<std::uint64_t>(faction_value(target)) << 32U) ^
        static_cast<std::uint64_t>(static_cast<std::int64_t>(observed_at)) ^ channel;
    const auto width = static_cast<std::uint64_t>(uncertainty) * 2U + 1U;
    const auto signed_error =
        static_cast<std::int64_t>(mix64(key) % width) - uncertainty;
    const auto estimate =
        static_cast<std::int64_t>(truth) +
        static_cast<std::int64_t>(truth) * signed_error / 10000;
    return saturating_int32(std::max<std::int64_t>(0, estimate));
}

void WorldDiplomacyState::observe_faction(FactionId observer, FactionId target,
                                          std::uint16_t uncertainty_permyriad,
                                          time::Tick now, std::int32_t distance) {
    if (observer == target || uncertainty_permyriad > 10000 || distance < 0) {
        throw std::invalid_argument{"情報觀測對象或誤差範圍無效"};
    }
    const auto truth = faction_truth(target);
    if (!truth.has_value()) {
        throw std::logic_error{"不可觀測缺席的勢力真值"};
    }
    if (!knowledge_.has_value()) {
        const auto extent = static_cast<std::size_t>(faction_count_) + 1U;
        knowledge_.emplace(extent * extent);
    }
    knowledge_->at(matrix_index(observer, target)) = {
        .military_power =
            estimated_power(observer, target, truth->military_power,
                            uncertainty_permyriad, now, UINT64_C(0x4d494c)),
        .economic_power =
            estimated_power(observer, target, truth->economic_power,
                            uncertainty_permyriad, now, UINT64_C(0x45434f)),
        .uncertainty_permyriad = uncertainty_permyriad,
        .observed_at = now,
        .distance = distance,
        .observed = true,
    };
}

void WorldDiplomacyState::observe_faction_by_distance(
    FactionId observer, FactionId target, std::int32_t distance,
    std::uint16_t relationship_quality_permyriad, time::Tick now) {
    if (distance < 0 || relationship_quality_permyriad > 10000) {
        throw std::invalid_argument{"情報距離或外交品質無效"};
    }
    const auto distance_error = std::min<std::int64_t>(8000, 1000 +
        static_cast<std::int64_t>(distance) * 350);
    const auto relationship_reduction =
        static_cast<std::int64_t>(relationship_quality_permyriad) * 3 / 5;
    const auto uncertainty = static_cast<std::uint16_t>(
        std::clamp<std::int64_t>(distance_error - relationship_reduction, 100, 10000));
    observe_faction(observer, target, uncertainty, now, distance);
}

ai::FactionView FactionViewFactory::make(const WorldDiplomacyState& world,
                                         FactionId observer) {
    const auto observer_index = world.faction_index(observer);
    const auto truth = world.faction_truth(observer);
    std::vector<ai::FactionEstimate> estimates;
    estimates.reserve(world.faction_count_ - 1U);
    for (std::size_t target_index = 1; target_index <= world.faction_count_;
         ++target_index) {
        if (target_index == observer_index) {
            continue;
        }
        const auto target = static_cast<FactionId>(target_index);
        if (!world.knowledge_.has_value()) {
            continue;
        }
        const auto& knowledge =
            world.knowledge_->at(world.matrix_index(observer, target));
        if (!knowledge.observed) {
            continue;
        }
        const auto& relation = world.relation(observer, target);
        estimates.push_back({
            .faction = static_cast<ai::FactionKey>(target_index),
            .military_power = knowledge.military_power,
            .economic_power = knowledge.economic_power,
            .uncertainty_permyriad = knowledge.uncertainty_permyriad,
            .observed_at = static_cast<std::int64_t>(knowledge.observed_at),
            .route_cost = knowledge.distance,
            .relation = {.favor = relation.favor,
                         .trust = relation.trust,
                         .fear = relation.fear,
                         .grievance = relation.grievance},
        });
    }
    return ai::FactionView{static_cast<ai::FactionKey>(observer_index),
                           truth.has_value() ? truth->military_power : 0,
                           truth.has_value() ? truth->economic_power : 0,
                           std::move(estimates)};
}

ai::FactionView make_faction_view(const WorldDiplomacyState& world,
                                  FactionId observer) {
    return FactionViewFactory::make(world, observer);
}

} // namespace aetheria::world
