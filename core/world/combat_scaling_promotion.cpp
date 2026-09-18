// 戰鬥事件升層的決定性配置與降層損失結算。

#include "core/world/combat_scaling.h"
#include "core/world/combat_scaling_detail.h"

#include <algorithm>
#include <array>
#include <type_traits>

#include "core/base/check.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::world {
using detail::kSideASalt;
using detail::kSideBSalt;
namespace {

constexpr std::uint64_t kPlacementSalt = UINT64_C(0x1c69b3f805e274ad);
constexpr std::int32_t kCohortPower = 10'000;
constexpr std::int32_t kMaximumCohortsPerSide = 32;

static_assert(std::has_unique_object_representations_v<PackedCombatPlacement>);

[[nodiscard]] std::int32_t cohort_count(std::int32_t remaining) noexcept {
    if (remaining <= 0) {
        return 0;
    }
    return static_cast<std::int32_t>(std::min<std::int64_t>(
        kMaximumCohortsPerSide,
        (static_cast<std::int64_t>(remaining) + kCohortPower - 1) / kCohortPower));
}

void append_side_placements(std::vector<PackedCombatPlacement>& placements,
                            std::uint64_t seed, std::uint8_t side,
                            std::int32_t count) {
    std::array<bool, 512> occupied{};
    for (std::int32_t index = 0; index < count; ++index) {
        const auto draw = worldgen::splitmix64(seed ^ static_cast<std::uint64_t>(index));
        auto edge_slot = static_cast<std::uint16_t>(draw % occupied.size());
        while (occupied[edge_slot]) {
            edge_slot = static_cast<std::uint16_t>((edge_slot + 1U) % occupied.size());
        }
        occupied[edge_slot] = true;
        const auto depth = static_cast<std::uint16_t>(edge_slot / 64U);
        const auto y = static_cast<std::uint16_t>(edge_slot % 64U);
        const auto x = static_cast<std::uint16_t>(side == 0U ? depth : 63U - depth);
        const auto packed = static_cast<std::uint64_t>(x) |
                            (static_cast<std::uint64_t>(y) << 16U) |
                            (static_cast<std::uint64_t>(side) << 32U);
        placements.push_back(
            {static_cast<std::uint64_t>(side) << 63U | static_cast<std::uint64_t>(index + 1),
             packed});
    }
}

}  // namespace

CombatPromotion promote_combat_event(const CombatEventState& state,
                                     std::uint64_t target_zone_id,
                                     std::int64_t promote_tick) {
    AETH_CHECK(state.event_id > 0);
    AETH_CHECK(state.initial_power_a > 0);
    AETH_CHECK(state.initial_power_b > 0);
    AETH_CHECK(state.accumulated_loss_a >= 0);
    AETH_CHECK(state.accumulated_loss_b >= 0);
    AETH_CHECK(state.accumulated_loss_a <= state.initial_power_a);
    AETH_CHECK(state.accumulated_loss_b <= state.initial_power_b);
    AETH_CHECK(promote_tick >= 0);
    auto seed = worldgen::splitmix64(state.event_id ^ target_zone_id ^ kPlacementSalt);
    seed = worldgen::splitmix64(seed ^ static_cast<std::uint64_t>(promote_tick));
    CombatPromotion result;
    result.named = state.named;
    append_side_placements(result.placements, seed ^ kSideASalt, 0,
                           cohort_count(state.initial_power_a - state.accumulated_loss_a));
    append_side_placements(result.placements, seed ^ kSideBSalt, 1,
                           cohort_count(state.initial_power_b - state.accumulated_loss_b));
    return result;
}

bool demote_combat_event(CombatEventState& state, const LayerCombatResult& result,
                         CombatExecutionCounters* counters) noexcept {
    AETH_CHECK(state.initial_power_a > 0);
    AETH_CHECK(state.initial_power_b > 0);
    if (result.resolution_id == state.last_resolution_id) {
        return false;
    }
    state.accumulated_loss_a = static_cast<std::int32_t>(std::min<std::int64_t>(
        state.initial_power_a,
        static_cast<std::int64_t>(state.accumulated_loss_a) + result.loss_a));
    state.accumulated_loss_b = static_cast<std::int32_t>(std::min<std::int64_t>(
        state.initial_power_b,
        static_cast<std::int64_t>(state.accumulated_loss_b) + result.loss_b));
    state.last_resolution_id = result.resolution_id;
    if (counters != nullptr && result.layer == CombatLayer::Site) {
        ++counters->site_reduction_writes;
    }
    return true;
}

}  // namespace aetheria::world
