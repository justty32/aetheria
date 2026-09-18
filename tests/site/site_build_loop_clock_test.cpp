#include "core/serialize/normalized_state_hash.h"
#include "core/site/site_build_loop.h"
#include "core/world/region_simulation.h"
#include "tests/site/site_build_loop_test_support.h"
#include "tests/support/performance.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::build_batch_fixture;
using aetheria::tests::build_fixture;
using aetheria::tests::kAbsentBuildCoordinate;
using aetheria::tests::kBuildCoordinate;
using aetheria::tests::kSecondBuildCoordinate;
using aetheria::tests::production_at;
using aetheria::tests::queue_layout;
using aetheria::tests::test_ruleset;
using aetheria::zone::InMemoryZoneStore;

TEST(SiteBuildLoop, SiteJoiningMidXunUsesTheSingleRegionClockBoundary) {
    auto fixture = build_batch_fixture();
    InMemoryZoneStore store{test_ruleset()};
    aetheria::site::SiteTurnPipeline pipeline{test_ruleset(), store};
    static_cast<void>(pipeline.advance_hours(fixture.first, fixture.region, 0,
                                             kBuildCoordinate, 36));
    EXPECT_EQ(aetheria::world::turn_clock(fixture.region).now,
              aetheria::time::Tick{} + aetheria::time::kHour * 36);

    const std::array targets{
        aetheria::site::SiteAdvanceTarget{&fixture.first, 0, kBuildCoordinate},
        aetheria::site::SiteAdvanceTarget{&fixture.second, 0, kSecondBuildCoordinate},
    };
    const auto report = pipeline.advance_hours(fixture.region, targets, 204);

    EXPECT_EQ(report.sites.size(), 2U);
    EXPECT_EQ(report.site_xun_boundaries, 2U);
    EXPECT_EQ(report.region_xun_advances, 1U);
    EXPECT_EQ(aetheria::world::turn_clock(fixture.region).now,
              aetheria::time::Tick{} + aetheria::time::kXun);
}

}  // namespace
