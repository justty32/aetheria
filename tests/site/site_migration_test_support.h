#pragma once

#include "core/site/site_projection.h"
#include "tests/site/site_reduction_test_support.h"

namespace aetheria::tests::site_migration_support {

using aetheria::site::SiteSkeleton;

[[nodiscard]] inline SiteSkeleton skeleton_for(const aetheria::world::RegionTiles& tiles) {
    return aetheria::site::build_site_skeleton(
        aetheria::site::split_site_vars(tiles, kReductionCoordinate).slow,
        aetheria::site::derive_site_seed(kReductionWorldSeed, kReductionRegionId, 0, 0),
        test_ruleset());
}

}  // namespace aetheria::tests::site_migration_support
