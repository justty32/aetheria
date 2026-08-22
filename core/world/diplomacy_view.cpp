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
    truths_.at(faction_index(faction)) = {military_power, economic_power};
}

std::int32_t WorldDiplomacyState::estimated_power(
    FactionId observer, FactionId target, std::int32_t truth,
    std::uint16_t uncertainty, std::uint64_t channel) const noexcept {
    const auto key =
        world_seed_ ^
        (static_cast<std::uint64_t>(faction_value(observer)) << 48U) ^
        (static_cast<std::uint64_t>(faction_value(target)) << 32U) ^ channel;
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
                                          time::Tick now) {
    if (observer == target || uncertainty_permyriad > 10000) {
        throw std::invalid_argument{"情報觀測對象或誤差範圍無效"};
    }
    const auto truth = truths_.at(faction_index(target));
    knowledge_.at(matrix_index(observer, target)) = {
        .military_power =
            estimated_power(observer, target, truth.military_power,
                            uncertainty_permyriad, UINT64_C(0x4d494c)),
        .economic_power =
            estimated_power(observer, target, truth.economic_power,
                            uncertainty_permyriad, UINT64_C(0x45434f)),
        .uncertainty_permyriad = uncertainty_permyriad,
        .observed_at = now,
    };
}

ai::FactionView FactionViewFactory::make(const WorldDiplomacyState& world,
                                         FactionId observer) {
    const auto observer_index = world.faction_index(observer);
    const auto truth = world.truths_.at(observer_index);
    std::vector<ai::FactionEstimate> estimates;
    estimates.reserve(world.faction_count_ - 1U);
    for (std::size_t target_index = 1; target_index <= world.faction_count_;
         ++target_index) {
        if (target_index == observer_index) {
            continue;
        }
        const auto target = static_cast<FactionId>(target_index);
        const auto& knowledge =
            world.knowledge_.at(world.matrix_index(observer, target));
        const auto& relation = world.relation(observer, target);
        estimates.push_back({
            .faction = static_cast<ai::FactionKey>(target_index),
            .military_power = knowledge.military_power,
            .economic_power = knowledge.economic_power,
            .uncertainty_permyriad = knowledge.uncertainty_permyriad,
            .observed_at = static_cast<std::int64_t>(knowledge.observed_at),
            .relation = {.favor = relation.favor,
                         .trust = relation.trust,
                         .fear = relation.fear,
                         .grievance = relation.grievance},
        });
    }
    return ai::FactionView{static_cast<ai::FactionKey>(observer_index),
                           truth.military_power, truth.economic_power,
                           std::move(estimates)};
}

ai::FactionView make_faction_view(const WorldDiplomacyState& world,
                                  FactionId observer) {
    return FactionViewFactory::make(world, observer);
}

} // namespace aetheria::world
