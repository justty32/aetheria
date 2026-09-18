#include "core/local/local_movement.h"
#include "core/local/local_path.h"
#include "tests/local/local_navigation_test_support.h"
#include "tests/support/performance.h"

#include <aetheria/runtime/cross_zone.h>

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>

namespace {

using aetheria::local::DoorState;
using aetheria::local::ExplorationStepResult;
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

TEST(LocalPath, WallBetweenWalkableTilesForcesFourStepDetour) {
  auto zone = navigation_zone(kNavigationCenter);
  const auto wall = *test_ruleset().find_edge("edge.house_wall");
  set_navigation_edge(*zone, {10, 10}, BoundarySide::East, wall);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};

  const auto path = aetheria::local::find_local_path(
      runtime, test_ruleset(), {kNavigationCenter, {10, 10}},
      {kNavigationCenter, {12, 10}});

  ASSERT_EQ(path.status, aetheria::local::LocalPathStatus::Found);
  EXPECT_EQ(path.steps.size(), 5U);
  EXPECT_EQ(path.cost, 4);
  EXPECT_NE(path.steps[1].location.horizontal.tile,
            (aetheria::local::LocalXY{11, 10}));
  std::cout << "local_path_edge_steps=" << path.steps.size()
            << " cost=" << path.cost << '\n';
}

TEST(LocalPath, ClosedDoorIsMarkedAndLockedDoorUsesDetour) {
  auto zone = navigation_zone(kNavigationCenter);
  const auto door = *test_ruleset().find_edge("edge.house_door");
  set_navigation_edge(*zone, {20, 20}, BoundarySide::East, door);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};
  const aetheria::local::LocalLocation start{kNavigationCenter, {20, 20}};
  const aetheria::local::LocalLocation goal{kNavigationCenter, {22, 20}};

  const auto closed =
      aetheria::local::find_local_path(runtime, test_ruleset(), start, goal,
                                       constant_door_state(DoorState::Closed));
  const auto open =
      aetheria::local::find_local_path(runtime, test_ruleset(), start, goal,
                                       constant_door_state(DoorState::Open));
  const auto locked =
      aetheria::local::find_local_path(runtime, test_ruleset(), start, goal,
                                       constant_door_state(DoorState::Locked));

  ASSERT_EQ(closed.status, aetheria::local::LocalPathStatus::Found);
  ASSERT_EQ(open.status, aetheria::local::LocalPathStatus::Found);
  ASSERT_EQ(locked.status, aetheria::local::LocalPathStatus::Found);
  EXPECT_EQ(closed.steps.size(), 3U);
  EXPECT_EQ(closed.steps[1].interaction,
            aetheria::local::LocalPathInteraction::OpenDoor);
  EXPECT_EQ(open.steps[1].interaction,
            aetheria::local::LocalPathInteraction::None);
  EXPECT_EQ(locked.steps.size(), 5U);
  EXPECT_TRUE(std::ranges::none_of(locked.steps, [](const auto &step) {
    return step.interaction == aetheria::local::LocalPathInteraction::OpenDoor;
  }));
}

TEST(LocalPath, EnclosedGoalIsNoPathRatherThanEmptySuccessfulPath) {
  auto zone = navigation_zone(kNavigationCenter);
  const auto wall = *test_ruleset().find_edge("edge.house_wall");
  constexpr aetheria::local::LocalXY goal_tile{30, 30};
  set_navigation_edge(*zone, goal_tile, BoundarySide::North, wall);
  set_navigation_edge(*zone, goal_tile, BoundarySide::East, wall);
  set_navigation_edge(*zone, goal_tile, BoundarySide::South, wall);
  set_navigation_edge(*zone, goal_tile, BoundarySide::West, wall);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};

  const auto path = aetheria::local::find_local_path(
      runtime, test_ruleset(), {kNavigationCenter, {28, 30}},
      {kNavigationCenter, goal_tile});

  EXPECT_EQ(path.status, aetheria::local::LocalPathStatus::NoPath);
  EXPECT_TRUE(path.steps.empty());
  EXPECT_GT(path.expanded_tiles, 0U);
}

TEST(LocalPath, GroundCostSelectsNoCostlierThanHandBuiltAlternative) {
  auto zone = navigation_zone(kNavigationCenter);
  auto &tiles =
      std::get<aetheria::zone::LocalPayload>(zone->payload).layers.at(0);
  const auto mud = *test_ruleset().find_ground("ground.mud");
  for (std::uint16_t x = 11; x <= 13; ++x) {
    tiles.ground[aetheria::tests::navigation_tile_index({x, 10})] = mud;
  }
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};

  const auto path = aetheria::local::find_local_path(
      runtime, test_ruleset(), {kNavigationCenter, {10, 10}},
      {kNavigationCenter, {14, 10}});

  ASSERT_EQ(path.status, aetheria::local::LocalPathStatus::Found);
  constexpr std::int64_t direct_cost = 7;
  constexpr std::int64_t hand_built_detour_cost = 6;
  EXPECT_LE(path.cost, direct_cost);
  EXPECT_LE(path.cost, hand_built_detour_cost);
  EXPECT_EQ(path.cost, hand_built_detour_cost);
}

TEST(LocalPath, IsDeterministicAndMeetsWarmMinOfFiveBudget) {
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};
  const auto measure_path = [&] {
    return aetheria::local::find_local_path(runtime, test_ruleset(),
                                            {kNavigationCenter, {1, 1}},
                                            {kNavigationCenter, {62, 62}});
  };
  const auto first = measure_path();
  const auto second = measure_path();
  EXPECT_EQ(first, second);

  aetheria::local::LocalPathResult measured;
  const auto measure = [&] {
    const auto start = std::chrono::steady_clock::now();
    measured = measure_path();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
  };
  const auto minimum =
      aetheria::tests::minimum_milliseconds_after_warmup(measure);

  ASSERT_EQ(measured.status, aetheria::local::LocalPathStatus::Found);
  EXPECT_GT(measured.expanded_tiles, 0U);
  EXPECT_GT(measured.steps.size(), 0U);
  EXPECT_LT(minimum, 10.0);
  std::cout << "local_path_min_of_5_ms=" << minimum
            << " expanded_tiles=" << measured.expanded_tiles
            << " path_steps=" << measured.steps.size() << '\n';
}

} // namespace
