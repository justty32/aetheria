#include "core/local/local_combat.h"

#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

[[nodiscard]] aetheria::spatial::BoundarySide
broken_local_choice(std::uint64_t seed) noexcept {
  return (seed & 1U) == 0U ? aetheria::spatial::BoundarySide::North
                           : aetheria::spatial::BoundarySide::South;
}

TEST(LocalCombatNegative, LocalChoosingEntryIgnoresSiteEastAndMustFail) {
  const aetheria::local::SiteCombatBoundaryCondition from_site{
      aetheria::spatial::BoundarySide::West,
      aetheria::spatial::BoundarySide::East,
      aetheria::spatial::BoundarySide::West,
      aetheria::spatial::BoundarySide::East};
  // 故障注入：Local 用 seed 自選入口，刻意不讀 Site 的 B 方 East。
  const auto actual = broken_local_choice(40);
  const auto expected = from_site.entry(aetheria::local::LocalCombatSide::B);
  std::cout << "local_boundary_negative expected_site_edge="
            << static_cast<int>(expected)
            << " locally_chosen_edge=" << static_cast<int>(actual) << '\n';
  EXPECT_EQ(actual, expected) << "Local 不得自行決定 Site 已指定的入口邊緣";
}

TEST(LocalCombatNegative, BypassingM5WallOcclusionInventsAnAttackAndMustFail) {
  // 故障注入：假裝 FOV 穿牆，逐單位戰鬥測試必須抓到多出來的一次攻擊。
  constexpr std::size_t broken_attacks = 1;
  std::cout << "local_m5_negative expected_wall_attacks=0 broken_attacks="
            << broken_attacks << '\n';
  EXPECT_EQ(broken_attacks, 0U) << "改壞 M5 FOV 後 Local 戰鬥不得維持綠燈";
}

} // namespace
