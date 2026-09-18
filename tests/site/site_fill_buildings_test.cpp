#include "tests/site/site_fill_test_support.h"
#include "tests/support/performance.h"

#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::site::SiteEdgeRef;
using aetheria::site::SiteFastVars;
using aetheria::site::SiteZoning;
using aetheria::tests::fill_counts;
using aetheria::tests::fill_slow_vars;
using aetheria::tests::fill_tile_index;
using aetheria::tests::frontage_is_road;
using aetheria::tests::generate_fill;
using aetheria::tests::test_ruleset;

TEST(SiteFill, LandmarkComesFromOwnerStyleAndUsesNearestCommercialBlock) {
    SiteFastVars fast{};
    fast.owner = static_cast<aetheria::world::FactionId>(1);
    fast.settlement = aetheria::world::SettlementTier::City;
    fast.population = 8000;
    fast.development_level = 20;
    const auto layer = generate_fill(fast);

    std::vector<const aetheria::site::ProceduralBuilding*> landmarks;
    for (const auto& building : layer.buildings) {
        if (test_ruleset().building(building.def)->landmark) {
            landmarks.push_back(&building);
        }
    }
    ASSERT_EQ(landmarks.size(), 1U);
    const auto& style = *std::ranges::find(test_ruleset().site_fill_rules().faction_styles, 1U,
                                           &aetheria::rules::FactionLandmarkStyle::faction);
    EXPECT_NE(std::ranges::find(style.landmarks, landmarks.front()->def), style.landmarks.end());

    auto landmark_distance = std::numeric_limits<std::uint32_t>::max();
    auto nearest_commercial = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t index = 0; index < layer.skeleton.blocks.size(); ++index) {
        if (layer.block_zoning[index] != SiteZoning::Commercial) {
            continue;
        }
        const auto& block = layer.skeleton.blocks[index];
        const auto center_x = static_cast<std::int32_t>(block.origin.x) + block.width / 2;
        const auto center_y = static_cast<std::int32_t>(block.origin.y) + block.height / 2;
        const auto distance = static_cast<std::uint32_t>(
            std::abs(center_x - static_cast<std::int32_t>(layer.skeleton.city_center.x)) +
            std::abs(center_y - static_cast<std::int32_t>(layer.skeleton.city_center.y)));
        nearest_commercial = std::min(nearest_commercial, distance);
        const auto& landmark = *landmarks.front();
        if (landmark.origin.x >= block.origin.x && landmark.origin.y >= block.origin.y &&
            landmark.origin.x + landmark.width <= block.origin.x + block.width &&
            landmark.origin.y + landmark.height <= block.origin.y + block.height) {
            landmark_distance = distance;
        }
    }
    EXPECT_EQ(landmark_distance, nearest_commercial);
}

TEST(SiteFill, BuildingsAreDisjointBuildableStreetFrontageWithCourtyards) {
    SiteFastVars fast{};
    fast.settlement = aetheria::world::SettlementTier::City;
    fast.population = 8000;
    fast.development_level = 20;
    const auto layer = generate_fill(fast);
    std::vector<std::uint8_t> occupied(aetheria::site::kSiteTileCount);
    std::size_t occupied_area{};
    std::size_t zoned_area{};
    for (std::size_t index = 0; index < layer.zoning.size(); ++index) {
        zoned_area += layer.zoning[index] != SiteZoning::Open ? 1U : 0U;
    }
    for (const auto& building : layer.buildings) {
        EXPECT_TRUE(frontage_is_road(layer, building));
        for (std::uint16_t y = building.origin.y; y < building.origin.y + building.height; ++y) {
            for (std::uint16_t x = building.origin.x; x < building.origin.x + building.width; ++x) {
                const auto index = fill_tile_index(x, y);
                EXPECT_EQ(occupied[index], 0U);
                EXPECT_NE(layer.zoning[index], SiteZoning::Open);
                EXPECT_NE(layer.skeleton.buildable[index], 0U);
                occupied[index] = UINT8_C(1);
                ++occupied_area;
            }
        }
    }
    EXPECT_TRUE(layer.valid_layout());
    EXPECT_GT(layer.buildings.size(), 0U);
    EXPECT_LT(occupied_area, zoned_area);
}

}  // namespace
