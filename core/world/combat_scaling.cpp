// combat_scaling.cpp：三層戰鬥的零均值抽樣、個人貢獻配額與決定性展開。

#include "core/world/combat_scaling.h"
#include "core/world/combat_scaling_detail.h"

#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>

#include "core/base/check.h"
#include "core/site/site_combat.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::world {
using detail::kSideASalt;
using detail::kSideBSalt;
namespace {

constexpr std::int32_t kPermyriad = 10'000;
constexpr std::uint64_t kResolutionSalt = UINT64_C(0x83ef5aa1d7294c60);
[[nodiscard]] constexpr std::size_t layer_index(CombatLayer layer) noexcept {
    return static_cast<std::size_t>(layer);
}

[[nodiscard]] constexpr std::size_t significance_index(Significance significance) noexcept {
    return static_cast<std::size_t>(significance);
}

[[nodiscard]] std::int32_t phase_share(std::int32_t total, CombatPhase phase) noexcept {
    AETH_CHECK(total >= 0);
    AETH_CHECK(phase.count > 0);
    AETH_CHECK(phase.index < phase.count);
    const auto before = static_cast<std::int64_t>(total) * phase.index / phase.count;
    const auto after = static_cast<std::int64_t>(total) * (phase.index + 1U) / phase.count;
    return static_cast<std::int32_t>(after - before);
}

[[nodiscard]] std::int32_t vary_loss(std::int32_t base, std::int32_t capacity,
                                     std::int32_t spread_permyriad,
                                     std::uint64_t event_id, std::uint64_t sample_seed,
                                     std::uint64_t salt) noexcept {
    AETH_CHECK(base >= 0);
    AETH_CHECK(capacity > 0);
    AETH_CHECK(spread_permyriad >= 0);
    AETH_CHECK(spread_permyriad <= kPermyriad);
    const auto maximum_delta = static_cast<std::int32_t>(
        static_cast<std::int64_t>(base) * spread_permyriad / kPermyriad);
    if (maximum_delta == 0) {
        return base;
    }
    // 相鄰 seed (2k, 2k+1) 共用 magnitude、符號相反，供校準採 antithetic pairs；
    // 每一個 seed 仍是不同事件抽樣，單獨呼叫也完全決定。
    const auto pair = sample_seed >> 1U;
    const auto draw = worldgen::splitmix64(event_id ^ pair ^ salt);
    const auto magnitude = static_cast<std::int32_t>(
        draw % (static_cast<std::uint64_t>(maximum_delta) + 1U));
    const auto signed_delta = (sample_seed & 1U) == 0U ? magnitude : -magnitude;
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(base) + signed_delta, 0, capacity));
}

[[nodiscard]] std::uint64_t resolution_id(std::uint64_t event_id, std::uint64_t sample_seed,
                                          CombatLayer layer, CombatPhase phase) noexcept {
    auto value = worldgen::splitmix64(event_id ^ kResolutionSalt);
    value = worldgen::splitmix64(value ^ sample_seed);
    value = worldgen::splitmix64(value ^ static_cast<std::uint64_t>(layer));
    value = worldgen::splitmix64(value ^
                                (static_cast<std::uint64_t>(phase.count) << 32U) ^ phase.index);
    return value == 0 ? 1U : value;
}

void count_execution(CombatLayer layer, CombatExecutionCounters* counters) noexcept {
    if (counters == nullptr) {
        return;
    }
    switch (layer) {
        case CombatLayer::Region:
            ++counters->region_face_runs;
            ++counters->region_face_damage_writes;
            break;
        case CombatLayer::Site:
            ++counters->site_face_runs;
            break;
        case CombatLayer::Local:
            ++counters->local_face_runs;
            break;
    }
}

}  // namespace

std::int32_t contribution_delta_limit(Significance significance, std::int32_t opposing_power,
                                      const CombatScalingRules& scaling) noexcept {
    AETH_CHECK(opposing_power > 0);
    const auto index = significance_index(significance);
    AETH_CHECK(index < scaling.contribution_delta_permyriad.size());
    const auto permyriad = scaling.contribution_delta_permyriad[index];
    AETH_CHECK(permyriad >= 0);
    AETH_CHECK(permyriad <= kPermyriad);
    return static_cast<std::int32_t>(static_cast<std::int64_t>(opposing_power) * permyriad /
                                     kPermyriad);
}

ContributionAllocation apply_personal_contribution(
    std::int32_t statistical_loss, std::int32_t opposing_power,
    PersonalContribution contribution, const CombatScalingRules& scaling) noexcept {
    AETH_CHECK(opposing_power > 0);
    AETH_CHECK(statistical_loss >= 0);
    AETH_CHECK(statistical_loss <= opposing_power);
    ContributionAllocation result{};
    result.statistical_loss = statistical_loss;
    result.delta_limit =
        contribution_delta_limit(contribution.significance, opposing_power, scaling);
    result.requested_deviation = contribution.requested_loss_shift_b;
    const auto bounded = std::clamp(contribution.requested_loss_shift_b,
                                    -result.delta_limit, result.delta_limit);
    const auto final = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(statistical_loss) + bounded, 0, opposing_power);
    result.final_loss = static_cast<std::int32_t>(final);
    result.applied_deviation = result.final_loss - statistical_loss;
    result.named_loss = std::max(0, result.applied_deviation);
    result.unnamed_loss = result.final_loss - result.named_loss;
    AETH_CHECK(result.named_loss + result.unnamed_loss == result.final_loss);
    return result;
}

LayerCombatResult resolve_scaled_combat(
    const rules::CombatInput& input, const rules::CombatRules& combat_rules,
    CombatLayer layer, std::uint64_t event_id, std::uint64_t sample_seed,
    CombatPhase phase, PersonalContribution contribution,
    CombatExecutionCounters* counters, const CombatScalingRules& scaling) noexcept {
    AETH_CHECK(event_id > 0);
    const auto region = rules::resolve_region_combat(input, combat_rules);
    const auto expected_a = phase_share(region.loss_a, phase);
    const auto expected_b = phase_share(region.loss_b, phase);
    const auto index = layer_index(layer);
    AETH_CHECK(index < scaling.random_spread_permyriad.size());
    const auto spread = scaling.random_spread_permyriad[index];
    auto varied_a = expected_a;
    auto varied_b = expected_b;
    if (layer == CombatLayer::Site) {
        const auto site_result = site::simulate_site_battle({
            .side_a = input.side_a,
            .side_b = input.side_b,
            .region_expected_loss_a = expected_a,
            .region_expected_loss_b = expected_b,
            .modifier_scale = combat_rules.modifier_scale,
            .tactical_spread_permyriad = spread,
            .systematic_bias_permyriad = scaling.site_systematic_bias_permyriad,
            .event_id = event_id,
            .sample_seed = sample_seed,
        });
        varied_a = site_result.loss_a;
        varied_b = site_result.loss_b;
    } else {
        varied_a = vary_loss(expected_a, input.side_a.power, spread, event_id,
                             sample_seed, kSideASalt ^ index);
        varied_b = vary_loss(expected_b, input.side_b.power, spread, event_id,
                             sample_seed, kSideBSalt ^ index);
    }
    const auto allocation =
        layer == CombatLayer::Local
            ? apply_personal_contribution(varied_b, input.side_b.power, contribution, scaling)
            : apply_personal_contribution(varied_b, input.side_b.power, {}, scaling);
    count_execution(layer, counters);
    return {layer,
            resolution_id(event_id, sample_seed, layer, phase),
            expected_a,
            expected_b,
            varied_a,
            allocation.final_loss,
            allocation};
}

}  // namespace aetheria::world
