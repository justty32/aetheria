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

TEST(LocalPath, UnloadedGoalReturnsCoarsePathOrUnknownWithoutThrowing) {
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};
  const aetheria::local::LocalLocation start{kNavigationCenter, {63, 30}};
  const aetheria::local::LocalLocation goal{kNavigationEast, {10, 30}};

  EXPECT_NO_THROW({
    const auto unknown =
        aetheria::local::find_local_path(runtime, test_ruleset(), start, goal);
    EXPECT_EQ(unknown.status, aetheria::local::LocalPathStatus::Unknown);
    EXPECT_TRUE(unknown.steps.empty());
  });
  EXPECT_NO_THROW({
    const auto coarse = aetheria::local::find_local_path(
        runtime, test_ruleset(), start, goal, {},
        [](aetheria::zone::ZoneKey from, aetheria::zone::ZoneKey to) {
          return std::optional<std::vector<aetheria::zone::ZoneKey>>{
              std::vector{from, to}};
        });
    EXPECT_EQ(coarse.status, aetheria::local::LocalPathStatus::Coarse);
    EXPECT_EQ(coarse.coarse_zones, (std::vector{start.zone, goal.zone}));
    EXPECT_TRUE(coarse.steps.empty());
  });
}

TEST(LocalPath, LoadedNeighborUsesExactCrossZonePath) {
  auto center = navigation_zone(kNavigationCenter);
  auto east = navigation_zone(kNavigationEast);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(center)));
  static_cast<void>(manager.adopt(std::move(east)));
  CrossZoneRuntime runtime{manager};

  const auto path = aetheria::local::find_local_path(
      runtime, test_ruleset(), {kNavigationCenter, {63, 30}},
      {kNavigationEast, {1, 30}});

  ASSERT_EQ(path.status, aetheria::local::LocalPathStatus::Found);
  ASSERT_EQ(path.steps.size(), 3U);
  EXPECT_EQ(path.steps[1].location.horizontal,
            (aetheria::local::LocalLocation{kNavigationEast, {0, 30}}));
  EXPECT_TRUE(path.coarse_zones.empty());
}

TEST(LocalPath,
     VerticalCallbackConnectsAdjacentLayersWithoutGeneratorCoupling) {
  auto zone = navigation_zone(kNavigationCenter);
  auto upper =
      std::get<aetheria::zone::LocalPayload>(zone->payload).layers.at(0);
  std::get<aetheria::zone::LocalPayload>(zone->payload)
      .layers.emplace(-1, std::move(upper));
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  CrossZoneRuntime runtime{manager};
  auto queries = aetheria::local::make_ground_path_queries(runtime);
  const auto grass = *test_ruleset().find_ground("ground.grass");
  queries.peek_tile = [grass](aetheria::local::LocalPathLocation location)
      -> std::optional<aetheria::runtime::TileView> {
    if (location.horizontal.zone != kNavigationCenter ||
        location.horizontal.tile.x >= aetheria::local::kLocalWidth ||
        location.horizontal.tile.y >= aetheria::local::kLocalHeight ||
        (location.z != 0 && location.z != -1)) {
      return std::nullopt;
    }
    return aetheria::runtime::TileView{grass, aetheria::local::OverlayId::None,
                                       0, UINT8_MAX};
  };
  queries.peek_edge = [](aetheria::local::LocalPathLocation, BoundarySide) {
    return std::optional<aetheria::runtime::EdgeView>{
        {*test_ruleset().find_edge("edge.none")}};
  };
  queries.vertical_neighbors = [](aetheria::local::LocalPathLocation location) {
    if (location.horizontal.tile == aetheria::local::LocalXY{12, 12}) {
      return location.z == 0 ? std::vector<std::int8_t>{-1}
                             : std::vector<std::int8_t>{0};
    }
    return std::vector<std::int8_t>{};
  };

  const auto path = aetheria::local::find_local_path(
      queries, test_ruleset(), {{kNavigationCenter, {11, 12}}, 0},
      {{kNavigationCenter, {13, 12}}, -1});

  ASSERT_EQ(path.status, aetheria::local::LocalPathStatus::Found);
  EXPECT_EQ(path.steps.size(), 4U);
  EXPECT_EQ(path.steps[2].location, (aetheria::local::LocalPathLocation{
                                        {kNavigationCenter, {12, 12}}, -1}));
}

} // namespace
