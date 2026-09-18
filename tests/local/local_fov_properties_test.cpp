#include "core/local/local_fov.h"
#include "tests/local/local_navigation_test_support.h"
#include "tests/support/performance.h"

#include <aetheria/runtime/cross_zone.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

#include "tests/local/local_fov_test_support.h"

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

using namespace aetheria::tests::local_fov_support;

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
