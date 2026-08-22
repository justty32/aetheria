#include "core/local/local_fov.h"
#include "tests/local/local_navigation_test_support.h"
#include "tests/support/performance.h"

#include <aetheria/runtime/cross_zone.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::local::DoorState;
using aetheria::local::LocalLocation;
using aetheria::runtime::CrossZoneRuntime;
using aetheria::spatial::BoundarySide;
using aetheria::tests::constant_door_state;
using aetheria::tests::kNavigationCenter;
using aetheria::tests::kNavigationEast;
using aetheria::tests::navigation_zone;
using aetheria::tests::set_navigation_edge;
using aetheria::tests::test_ruleset;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::ZoneManager;

[[nodiscard]] bool visible(const aetheria::local::FovResult &result,
                           LocalLocation location) {
  return aetheria::local::is_visible(result, location);
}

TEST(LocalFov, PassableTilesSeparatedByWallEdgeDoNotSeeAcross) {
  auto zone = navigation_zone(kNavigationCenter);
  const auto wall = *test_ruleset().find_edge("edge.house_wall");
  set_navigation_edge(*zone, {32, 32}, BoundarySide::East, wall);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};

  const auto result = aetheria::local::calculate_fov(
      runtime, test_ruleset(), {kNavigationCenter, {32, 32}}, {4, 4});

  EXPECT_TRUE(visible(result, {kNavigationCenter, {32, 32}}));
  EXPECT_FALSE(visible(result, {kNavigationCenter, {33, 32}}));
  EXPECT_FALSE(result.degraded);
  std::cout << "fov_edge_negative_control_visible=" << result.visible.size()
            << '\n';
  std::cout << "fov_debug_map wall east of @\n";
  for (std::uint16_t y = 28; y <= 36; ++y) {
    for (std::uint16_t x = 28; x <= 36; ++x) {
      std::cout << (x == 32 && y == 32                             ? '@'
                    : visible(result, {kNavigationCenter, {x, y}}) ? '.'
                                                                   : ' ');
    }
    std::cout << '\n';
  }
}

TEST(LocalFov, ClosedAndLockedDoorsBlockButOpenDoorDoesNot) {
  auto zone = navigation_zone(kNavigationCenter);
  const auto door = *test_ruleset().find_edge("edge.house_door");
  set_navigation_edge(*zone, {20, 20}, BoundarySide::East, door);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};
  const LocalLocation origin{kNavigationCenter, {20, 20}};
  const LocalLocation target{kNavigationCenter, {21, 20}};

  const auto closed =
      aetheria::local::calculate_fov(runtime, test_ruleset(), origin, {2, 2},
                                     constant_door_state(DoorState::Closed));
  const auto open =
      aetheria::local::calculate_fov(runtime, test_ruleset(), origin, {2, 2},
                                     constant_door_state(DoorState::Open));
  const auto locked =
      aetheria::local::calculate_fov(runtime, test_ruleset(), origin, {2, 2},
                                     constant_door_state(DoorState::Locked));

  EXPECT_FALSE(visible(closed, target));
  EXPECT_TRUE(visible(open, target));
  EXPECT_FALSE(visible(locked, target));
}

TEST(LocalFov, EndpointLightChangesSymmetricVisibleDistance) {
  auto dark_zone = navigation_zone(kNavigationCenter, 0);
  InMemoryZoneStore dark_store{test_ruleset()};
  ZoneManager dark_manager{dark_store};
  static_cast<void>(dark_manager.adopt(std::move(dark_zone)));
  CrossZoneRuntime dark_runtime{dark_manager};
  const auto dark = aetheria::local::calculate_fov(
      dark_runtime, test_ruleset(), {kNavigationCenter, {32, 32}}, {1, 4});

  auto bright_zone = navigation_zone(kNavigationCenter, UINT8_MAX);
  InMemoryZoneStore bright_store{test_ruleset()};
  ZoneManager bright_manager{bright_store};
  static_cast<void>(bright_manager.adopt(std::move(bright_zone)));
  CrossZoneRuntime bright_runtime{bright_manager};
  const auto bright = aetheria::local::calculate_fov(
      bright_runtime, test_ruleset(), {kNavigationCenter, {32, 32}}, {1, 4});

  EXPECT_EQ(dark.visible.size(), 5U);
  EXPECT_EQ(bright.visible.size(), 49U);
  EXPECT_LT(dark.visible.size(), bright.visible.size());
}

TEST(LocalFov, LoadedNeighborIsVisibleAndAbsentNeighborDegradesToUnknown) {
  auto center = navigation_zone(kNavigationCenter);
  auto east = navigation_zone(kNavigationEast);
  InMemoryZoneStore loaded_store{test_ruleset()};
  ZoneManager loaded_manager{loaded_store};
  static_cast<void>(loaded_manager.adopt(std::move(center)));
  static_cast<void>(loaded_manager.adopt(std::move(east)));
  CrossZoneRuntime loaded_runtime{loaded_manager};
  const LocalLocation origin{kNavigationCenter, {63, 32}};
  const LocalLocation across{kNavigationEast, {0, 32}};

  const auto loaded = aetheria::local::calculate_fov(
      loaded_runtime, test_ruleset(), origin, {2, 2});
  EXPECT_TRUE(visible(loaded, across));
  EXPECT_FALSE(loaded.degraded);

  auto isolated = navigation_zone(kNavigationCenter);
  InMemoryZoneStore absent_store{test_ruleset()};
  ZoneManager absent_manager{absent_store};
  static_cast<void>(absent_manager.adopt(std::move(isolated)));
  CrossZoneRuntime absent_runtime{absent_manager};
  EXPECT_NO_THROW({
    const auto absent = aetheria::local::calculate_fov(
        absent_runtime, test_ruleset(), origin, {2, 2});
    EXPECT_FALSE(visible(absent, across));
    EXPECT_TRUE(absent.degraded);
    EXPECT_GT(absent.visible.size(), 0U);
  });
}

TEST(LocalFov, VisibilityIsPairwiseSymmetricAndDeterministic) {
  auto zone = navigation_zone(kNavigationCenter);
  const auto wall = *test_ruleset().find_edge("edge.house_wall");
  for (std::uint16_t y = 27; y <= 37; ++y) {
    set_navigation_edge(*zone, {32, y}, BoundarySide::East, wall);
  }
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};
  constexpr std::array<LocalLocation, 6> points{{
      {kNavigationCenter, {27, 27}},
      {kNavigationCenter, {31, 32}},
      {kNavigationCenter, {32, 38}},
      {kNavigationCenter, {33, 26}},
      {kNavigationCenter, {34, 32}},
      {kNavigationCenter, {38, 38}},
  }};

  std::array<aetheria::local::FovResult, points.size()> fields;
  std::size_t determinism_mismatches{};
  for (std::size_t index = 0; index < points.size(); ++index) {
    fields[index] = aetheria::local::calculate_fov(runtime, test_ruleset(),
                                                   points[index], {12, 12});
    const auto repeated = aetheria::local::calculate_fov(
        runtime, test_ruleset(), points[index], {12, 12});
    if (fields[index] != repeated) {
      ++determinism_mismatches;
    }
    EXPECT_EQ(fields[index], repeated);
  }
  std::size_t ordered_pairs{};
  std::size_t symmetry_mismatches{};
  for (std::size_t first = 0; first < points.size(); ++first) {
    for (std::size_t second = 0; second < points.size(); ++second) {
      const auto forward = visible(fields[first], points[second]);
      const auto reverse = visible(fields[second], points[first]);
      ++ordered_pairs;
      if (forward != reverse) {
        ++symmetry_mismatches;
      }
      EXPECT_EQ(forward, reverse);
    }
  }
  std::cout << "fov_determinism_mismatches=" << determinism_mismatches
            << " symmetry_ordered_pairs=" << ordered_pairs
            << " symmetry_mismatches=" << symmetry_mismatches << '\n';
}

TEST(LocalFov, RadiusCurveUsesWarmMinOfFiveAndTraversesNonEmptyFields) {
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};
  constexpr std::array<std::uint16_t, 5> radii{8, 12, 16, 24, 32};
  constexpr std::array<std::size_t, 5> expected_visible{197, 441, 797, 1'793,
                                                        3'207};
  for (std::size_t index = 0; index < radii.size(); ++index) {
    const auto radius = radii[index];
    std::size_t visible_count{};
    const auto measure = [&] {
      const auto start = std::chrono::steady_clock::now();
      const auto result = aetheria::local::calculate_fov(
          runtime, test_ruleset(), {kNavigationCenter, {32, 32}},
          {radius, radius});
      const auto end = std::chrono::steady_clock::now();
      visible_count = result.visible.size();
      return std::chrono::duration<double, std::milli>(end - start).count();
    };
    const auto minimum =
        aetheria::tests::minimum_milliseconds_after_warmup(measure);

    EXPECT_GT(visible_count, 0U);
    EXPECT_EQ(visible_count, expected_visible[index]);
    if (radius == 16) {
      EXPECT_LT(minimum, 2.0);
    }
    std::cout << "local_fov_radius=" << radius << " min_of_5_ms=" << minimum
              << " visible_tiles=" << visible_count << '\n';
  }
}

} // namespace
