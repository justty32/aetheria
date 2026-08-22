#include "core/local/local_movement.h"
#include "tests/local/local_navigation_test_support.h"

#include <aetheria/runtime/cross_zone.h>

#include <gtest/gtest.h>

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

TEST(LocalMovement, OpenEdgeAllowsFourNeighborExplorationAtMinuteStride) {
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};

  EXPECT_EQ(aetheria::local::assess_exploration_step(
                runtime, test_ruleset(), {kNavigationCenter, {10, 10}},
                BoundarySide::North),
            ExplorationStepResult::Allowed);
  static_assert(aetheria::local::kExplorationStride == aetheria::time::kMinute);
}

TEST(LocalMovement, WallBlocksAndDoorRequiresOpenUnlessLocked) {
  auto zone = navigation_zone(kNavigationCenter);
  const auto wall = *test_ruleset().find_edge("edge.house_wall");
  const auto door = *test_ruleset().find_edge("edge.house_door");
  set_navigation_edge(*zone, {10, 10}, BoundarySide::East, wall);
  set_navigation_edge(*zone, {20, 20}, BoundarySide::South, door);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};

  EXPECT_EQ(aetheria::local::assess_exploration_step(
                runtime, test_ruleset(), {kNavigationCenter, {10, 10}},
                BoundarySide::East),
            ExplorationStepResult::BlockedByWall);
  EXPECT_EQ(aetheria::local::assess_exploration_step(
                runtime, test_ruleset(), {kNavigationCenter, {20, 20}},
                BoundarySide::South, constant_door_state(DoorState::Closed)),
            ExplorationStepResult::MustOpenDoor);
  EXPECT_EQ(aetheria::local::assess_exploration_step(
                runtime, test_ruleset(), {kNavigationCenter, {20, 20}},
                BoundarySide::South, constant_door_state(DoorState::Open)),
            ExplorationStepResult::Allowed);
  EXPECT_EQ(aetheria::local::assess_exploration_step(
                runtime, test_ruleset(), {kNavigationCenter, {20, 20}},
                BoundarySide::South, constant_door_state(DoorState::Locked)),
            ExplorationStepResult::BlockedByLockedDoor);
}

TEST(LocalMovement, CrossZoneRequiresLoadedDestinationAndIsDeterministic) {
  auto center = navigation_zone(kNavigationCenter);
  InMemoryZoneStore absent_store{test_ruleset()};
  ZoneManager absent_manager{absent_store};
  static_cast<void>(absent_manager.adopt(std::move(center)));
  CrossZoneRuntime absent_runtime{absent_manager};
  const aetheria::local::LocalLocation from{kNavigationCenter, {63, 30}};

  EXPECT_NO_THROW({
    EXPECT_EQ(aetheria::local::assess_exploration_step(
                  absent_runtime, test_ruleset(), from, BoundarySide::East),
              ExplorationStepResult::Unknown);
  });

  auto loaded_center = navigation_zone(kNavigationCenter);
  auto east = navigation_zone(kNavigationEast);
  InMemoryZoneStore loaded_store{test_ruleset()};
  ZoneManager loaded_manager{loaded_store};
  static_cast<void>(loaded_manager.adopt(std::move(loaded_center)));
  static_cast<void>(loaded_manager.adopt(std::move(east)));
  CrossZoneRuntime loaded_runtime{loaded_manager};
  const auto first = aetheria::local::assess_exploration_step(
      loaded_runtime, test_ruleset(), from, BoundarySide::East);
  const auto second = aetheria::local::assess_exploration_step(
      loaded_runtime, test_ruleset(), from, BoundarySide::East);
  EXPECT_EQ(first, ExplorationStepResult::Allowed);
  EXPECT_EQ(first, second);
}

} // namespace
