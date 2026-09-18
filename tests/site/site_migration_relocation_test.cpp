#include "core/site/site_build_loop.h"
#include "core/site/site_lifecycle.h"
#include "core/site/site_materialize.h"
#include "tests/site/site_migration_test_support.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::site::PersistentBuilding;
using aetheria::site::SiteDigest;
using aetheria::site::SiteSkeleton;
using aetheria::site::SiteXY;
using aetheria::tests::kReductionCoordinate;
using aetheria::tests::kReductionRegionId;
using aetheria::tests::kReductionWorldSeed;
using aetheria::tests::reduction_region;
using aetheria::tests::test_ruleset;

using aetheria::tests::site_migration_support::skeleton_for;

[[nodiscard]] bool city_footprint_is_legal(const SiteSkeleton& skeleton, SiteXY origin,
                                           std::string_view definition_id) {
    const auto id = test_ruleset().find_city_building(definition_id);
    const auto* definition = id.has_value() ? test_ruleset().city_building(*id) : nullptr;
    if (definition == nullptr ||
        static_cast<std::uint32_t>(origin.x) + definition->width > aetheria::site::kSiteWidth ||
        static_cast<std::uint32_t>(origin.y) + definition->height > aetheria::site::kSiteHeight) {
        return false;
    }
    for (std::uint16_t y = origin.y; y < origin.y + definition->height; ++y) {
        for (std::uint16_t x = origin.x; x < origin.x + definition->width; ++x) {
            if (!skeleton.is_buildable({x, y})) {
                return false;
            }
        }
    }
    return true;
}

TEST(SiteMigration, RelocationWithoutDestructionStillEmitsNarrativeEvent) {
    const auto tiles = reduction_region();
    const auto skeleton = skeleton_for(tiles);
    std::vector<SiteXY> legal;
    SiteXY illegal{};
    bool found_illegal{};
    for (std::uint16_t y = 0; y < aetheria::site::kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < aetheria::site::kSiteWidth; ++x) {
            const SiteXY tile{x, y};
            if (skeleton.is_buildable(tile)) {
                legal.push_back(tile);
            } else if (!found_illegal) {
                illegal = tile;
                found_illegal = true;
            }
        }
    }
    ASSERT_GE(legal.size(), 2U);
    ASSERT_TRUE(found_illegal);
    SiteDigest digest;
    digest.skeleton_hash = aetheria::site::hash_site_skeleton(skeleton) ^ UINT64_C(1);
    digest.objects = {{legal.front()}, {illegal}};
    digest.city_buildings = {{"city.house", illegal}};
    digest.pending = {{"city.square", illegal, 5}};
    const auto report = aetheria::site::migrate_site_digest(digest, skeleton, test_ruleset());
    EXPECT_EQ(report.retained, 1U);
    EXPECT_EQ(report.relocated, 3U);
    EXPECT_EQ(report.destroyed, 0U);
    EXPECT_EQ(report.events_generated, 1U);
    ASSERT_EQ(digest.migration.events.size(), 1U);
    EXPECT_FALSE(digest.migration.events.front().narrative.empty());
    EXPECT_TRUE(std::ranges::all_of(digest.objects, [&](const PersistentBuilding& building) {
        return skeleton.is_buildable(building.tile);
    }));
    EXPECT_TRUE(std::ranges::all_of(digest.city_buildings, [&](const auto& building) {
        return city_footprint_is_legal(skeleton, building.origin, building.definition_id);
    }));
    EXPECT_TRUE(std::ranges::all_of(digest.pending, [&](const auto& construction) {
        return city_footprint_is_legal(skeleton, construction.origin, construction.definition_id);
    }));
}

}  // namespace
