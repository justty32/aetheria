#pragma once

#include "core/local/local_fov.h"
#include "tests/local/local_navigation_test_support.h"
#include "tests/support/performance.h"

#include <aetheria/runtime/cross_zone.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace aetheria::tests::local_fov_support {

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

[[nodiscard]] inline bool visible(const aetheria::local::FovResult &result,
                           LocalLocation location) {
  return aetheria::local::is_visible(result, location);
}

} // namespace aetheria::tests::local_fov_support
