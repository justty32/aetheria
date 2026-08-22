#pragma once

// local_combat.h：L3 逐單位戰鬥、Site 邊界輸入與三層撤退／士氣歸約。

#include "core/local/local_navigation.h"
#include "core/time/tick.h"
#include "core/world/combat_scaling.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace aetheria::rules {
class Ruleset;
}

namespace aetheria::runtime {
class CrossZoneRuntime;
}

namespace aetheria::local {

inline constexpr time::Duration kCombatStride = time::kLocalCombatTurn;
inline constexpr std::size_t kCombatMovementTiles = 5;

enum class LocalCombatSide : std::uint8_t { A, B };

[[nodiscard]] constexpr std::size_t side_index(LocalCombatSide side) noexcept {
  return static_cast<std::size_t>(side);
}

// 只能由呼叫端（Site 面）完整提供，Local 沒有自行挑邊的入口。
class SiteCombatBoundaryCondition {
public:
  constexpr SiteCombatBoundaryCondition(
      spatial::BoundarySide side_a_entry, spatial::BoundarySide side_b_entry,
      spatial::BoundarySide side_a_retreat,
      spatial::BoundarySide side_b_retreat) noexcept
      : entries_{side_a_entry, side_b_entry},
        retreats_{side_a_retreat, side_b_retreat} {}

  SiteCombatBoundaryCondition() = delete;

  [[nodiscard]] constexpr spatial::BoundarySide
  entry(LocalCombatSide side) const noexcept {
    return entries_[side_index(side)];
  }

  [[nodiscard]] constexpr spatial::BoundarySide
  retreat(LocalCombatSide side) const noexcept {
    return retreats_[side_index(side)];
  }

private:
  std::array<spatial::BoundarySide, 2> entries_;
  std::array<spatial::BoundarySide, 2> retreats_;
};

struct LocalCombatStats {
  std::int32_t accuracy{65};
  std::int32_t evasion{20};
  std::int32_t damage{10};
  std::uint16_t vision{12};

  constexpr bool operator==(const LocalCombatStats &) const noexcept = default;
};

struct LocalCombatant {
  std::uint64_t unit_id{};
  LocalCombatSide side{LocalCombatSide::A};
  LocalLocation location{};
  world::Significance significance{world::Significance::Ambient};
  LocalCombatStats stats{};
  std::int32_t health{20};
  bool commander{};
  bool banner{};
  bool alive{true};

  constexpr bool operator==(const LocalCombatant &) const noexcept = default;
};

struct SharedMorale {
  std::int32_t local{80};
  std::int32_t site{80};
  std::int32_t region{80};

  constexpr bool operator==(const SharedMorale &) const noexcept = default;
};

enum class LayerCombatState : std::uint8_t {
  Engaged,
  Retreated,
  Pursuing,
  Routed,
  Annihilated,
};

struct RoutedRemnantCohort {
  std::int32_t headcount{};
  // 聚合只提升「被個別計算的資格」，不改任何戰鬥位階。
  world::Significance aggregation_significance{world::Significance::Region};

  constexpr bool
  operator==(const RoutedRemnantCohort &) const noexcept = default;
};

struct CombatLayerSummary {
  LayerCombatState local{LayerCombatState::Engaged};
  LayerCombatState site{LayerCombatState::Engaged};
  LayerCombatState region{LayerCombatState::Engaged};
  std::optional<spatial::BoundarySide> region_retreat_direction;
  std::optional<RoutedRemnantCohort> routed_remnant;

  constexpr bool
  operator==(const CombatLayerSummary &) const noexcept = default;
};

struct LocalCombatState {
  std::vector<LocalCombatant> combatants;
  std::array<SharedMorale, 2> morale{};
  std::array<CombatLayerSummary, 2> layers{};
  std::uint32_t round_index{};

  bool operator==(const LocalCombatState &) const = default;
};

struct LocalPersonalAction {
  std::uint64_t unit_id{};
  std::int32_t requested_loss_shift{};

  constexpr bool
  operator==(const LocalPersonalAction &) const noexcept = default;
};

struct LocalCombatRoundInput {
  // M7.2 尚未整合時，這兩個值由 M6.7 的 Site 面提供。
  std::array<std::int32_t, 2> site_expected_loss{};
  std::array<std::int32_t, 2> side_power{};
  std::array<bool, 2> retreat_requested{};
  std::array<bool, 2> pursue_retreat{};
  std::vector<LocalPersonalAction> personal_actions;
};

struct LocalContributionResult {
  std::uint64_t unit_id{};
  LocalCombatSide side{LocalCombatSide::A};
  world::ContributionAllocation allocation{};

  constexpr bool
  operator==(const LocalContributionResult &) const noexcept = default;
};

struct LocalCombatRoundResult {
  std::array<std::int32_t, 2> loss{};
  std::array<std::size_t, 2> moved_tiles{};
  std::array<std::size_t, 2> attacks{};
  std::array<std::size_t, 2> hits{};
  std::array<std::int32_t, 2> pursuit_loss{};
  std::vector<LocalContributionResult> contributions;
  std::array<CombatLayerSummary, 2> layers{};

  bool operator==(const LocalCombatRoundResult &) const = default;
};

enum class LocalMoraleEvent : std::uint8_t {
  CommanderKilled,
  BannerCaptured,
  AlliesRouted,
};

// 位置只由 boundary.entry(side) 決定；同輸入順序與 id 得到同佈局。
void deploy_local_combatants(LocalCombatState &state, zone::ZoneKey battlefield,
                             const SiteCombatBoundaryCondition &boundary);

void apply_local_morale_event(LocalCombatState &state, LocalCombatSide side,
                              LocalMoraleEvent event) noexcept;

[[nodiscard]] LocalCombatRoundResult resolve_local_combat_round(
    LocalCombatState &state, const runtime::CrossZoneRuntime &runtime,
    const rules::Ruleset &ruleset, const SiteCombatBoundaryCondition &boundary,
    const LocalCombatRoundInput &input, std::uint64_t seed);

} // namespace aetheria::local
