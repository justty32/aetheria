#include "core/site/site_build_loop.h"
#include "core/site/site_lifecycle.h"
#include "core/site/site_materialize.h"
#include "tests/site/site_reduction_test_support.h"

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

constexpr auto kRegionKey =
    aetheria::zone::child_key(aetheria::zone::kRootZone, kReductionRegionId, 0);
constexpr auto kSiteKey = aetheria::zone::child_key(kRegionKey, 0, 0);

[[nodiscard]] std::size_t tile_index(SiteXY tile) {
    return static_cast<std::size_t>(tile.y) * aetheria::site::kSiteWidth + tile.x;
}

[[nodiscard]] SiteSkeleton skeleton_for(const aetheria::world::RegionTiles& tiles) {
    return aetheria::site::build_site_skeleton(
        aetheria::site::split_site_vars(tiles, kReductionCoordinate).slow,
        aetheria::site::derive_site_seed(kReductionWorldSeed, kReductionRegionId, 0, 0),
        test_ruleset());
}

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

struct ChangedSkeleton {
    aetheria::rules::ReliefId relief;
    SiteSkeleton skeleton;
    std::vector<SiteXY> old_legal_new_illegal;
    std::vector<SiteXY> legal;
};

[[nodiscard]] ChangedSkeleton find_changed_skeleton(const aetheria::world::RegionTiles& tiles,
                                                    const SiteSkeleton& old_skeleton) {
    for (const auto& definition : test_ruleset().reliefs()) {
        const auto id = *test_ruleset().find_relief(definition.id);
        if (id == tiles.relief[0]) {
            continue;
        }
        auto changed_tiles = tiles;
        changed_tiles.relief[0] = id;
        auto changed = skeleton_for(changed_tiles);
        std::vector<SiteXY> invalidated;
        std::vector<SiteXY> legal;
        for (std::uint16_t y = 0; y < aetheria::site::kSiteHeight; ++y) {
            for (std::uint16_t x = 0; x < aetheria::site::kSiteWidth; ++x) {
                const SiteXY tile{x, y};
                if (changed.is_buildable(tile)) {
                    legal.push_back(tile);
                } else if (old_skeleton.is_buildable(tile)) {
                    invalidated.push_back(tile);
                }
            }
        }
        if (invalidated.size() >= 2 && legal.size() >= 2 &&
            aetheria::site::hash_site_skeleton(old_skeleton) !=
                aetheria::site::hash_site_skeleton(changed)) {
            return {id, std::move(changed), std::move(invalidated), std::move(legal)};
        }
    }
    throw std::runtime_error{"測試 ruleset 找不到會讓舊可建地失效的 relief 變化"};
}

TEST(SiteMigration, SlowVariableChangeRetainsRelocatesAndExplicitlyDestroysWithoutIllegalTiles) {
    auto tiles = reduction_region();
    tiles.owner[0] = static_cast<aetheria::world::FactionId>(1);
    auto site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
    auto& state = aetheria::site::city_build_state(site);
    state.buildings.clear();
    state.pending.clear();

    const auto old_skeleton =
        std::get<aetheria::zone::SitePayload>(site.payload).layers.procedural.skeleton;
    auto changed = find_changed_skeleton(tiles, old_skeleton);
    const auto deliberately_free = changed.legal.front();
    auto& objects = std::get<aetheria::zone::SitePayload>(site.payload).layers.persistent.buildings;
    objects.clear();
    for (const auto tile : changed.legal) {
        if (tile != deliberately_free) {
            objects.push_back({tile});
        }
    }
    objects.push_back({changed.old_legal_new_illegal[0]});
    objects.push_back({changed.old_legal_new_illegal[1]});
    const auto original_object_count = objects.size();
    ASSERT_EQ(original_object_count, changed.legal.size() + 1U);

    const auto illegal_without_migration = static_cast<std::size_t>(
        std::ranges::count_if(objects, [&](const PersistentBuilding& building) {
            return !changed.skeleton.is_buildable(building.tile);
        }));
    ASSERT_EQ(illegal_without_migration, 2U) << "負向控制：直接沿用舊座標必須真的留下非法物件";

    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(site);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    auto handle = aetheria::site::rematerialize_site_zone(manager, tiles, kReductionCoordinate,
                                                          kReductionWorldSeed, kReductionRegionId,
                                                          test_ruleset());
    ASSERT_TRUE(manager.with(handle, [&](aetheria::zone::Zone& loaded) {
        aetheria::site::enter_full_site(loaded, tiles, kReductionCoordinate);
    }));
    aetheria::site::unload_site_zone(manager, handle, tiles, kReductionCoordinate,
                                     kReductionWorldSeed, kReductionRegionId,
                                     aetheria::time::Tick{});
    ASSERT_FALSE(manager.get(kSiteKey).has_value());

    tiles.relief[0] = changed.relief;
    aetheria::site::SiteCatchUpReport report;
    handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        aetheria::time::Tick{}, test_ruleset(), &report);
    ASSERT_TRUE(report.migration.applied);
    EXPECT_EQ(report.migration.retained, changed.legal.size() - 1U);
    EXPECT_EQ(report.migration.relocated, 1U);
    EXPECT_EQ(report.migration.destroyed, 1U);
    EXPECT_EQ(report.migration.events_generated, 1U);

    ASSERT_TRUE(manager.with(handle, [&](const aetheria::zone::Zone& loaded) {
        const auto& loaded_layers = std::get<aetheria::zone::SitePayload>(loaded.payload).layers;
        const auto& loaded_state = aetheria::site::city_build_state(loaded);
        std::array<std::uint8_t, aetheria::site::kSiteTileCount> occupied{};
        for (const auto& building : loaded_layers.persistent.buildings) {
            ASSERT_TRUE(loaded_layers.procedural.skeleton.is_buildable(building.tile));
            ASSERT_EQ(occupied[tile_index(building.tile)], 0U);
            occupied[tile_index(building.tile)] = UINT8_C(1);
        }
        EXPECT_EQ(loaded_layers.persistent.buildings.size(), changed.legal.size());
        ASSERT_EQ(loaded_state.migration.destroyed_objects.size(), 1U);
        ASSERT_EQ(loaded_state.migration.events.size(), 1U);
        EXPECT_FALSE(loaded_state.migration.events.front().narrative.empty());
        EXPECT_EQ(loaded_layers.persistent.buildings.size() +
                      loaded_state.migration.destroyed_objects.size(),
                  original_object_count);
    }));
    aetheria::site::unload_site_zone(manager, handle, tiles, kReductionCoordinate,
                                     kReductionWorldSeed, kReductionRegionId,
                                     aetheria::time::Tick{});
    aetheria::site::SiteCatchUpReport second_report;
    handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        aetheria::time::Tick{}, test_ruleset(), &second_report);
    EXPECT_FALSE(second_report.migration.applied);
    ASSERT_TRUE(manager.with(handle, [](const aetheria::zone::Zone& loaded) {
        const auto& loaded_state = aetheria::site::city_build_state(loaded);
        EXPECT_EQ(loaded_state.migration.destroyed_objects.size(), 1U);
        EXPECT_EQ(loaded_state.migration.events.size(), 1U);
    }));
    std::cout << "site_skeleton_migration retained=" << report.migration.retained
              << " relocated=" << report.migration.relocated
              << " destroyed=" << report.migration.destroyed
              << " events=" << report.migration.events_generated
              << " negative_control_illegal=" << illegal_without_migration << '\n';
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
