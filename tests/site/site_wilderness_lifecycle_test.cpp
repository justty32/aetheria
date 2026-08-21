#include "core/serialize/all_components.h"
#include "core/serialize/zone_codec.h"
#include "core/site/site_materialize.h"
#include "core/site/site_wilderness.h"
#include "core/zone/file_zone_store.h"
#include "tests/site/site_wilderness_test_support.h"
#include "tests/zone/zone_test_support.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include <entt/core/type_traits.hpp>
#include <gtest/gtest.h>

namespace {

using aetheria::serialize::AllComponents;
using aetheria::site::CityBuildState;
using aetheria::site::SitePosition;
using aetheria::site::WildernessEncounterPoint;
using aetheria::site::WildernessPortal;
using aetheria::site::WildernessResourcePoint;
using aetheria::site::WildernessTravelerPoint;
using aetheria::site::WildernessVegetation;
using aetheria::tests::add_crossings;
using aetheria::tests::kWildCenter;
using aetheria::tests::kWildRegionId;
using aetheria::tests::kWildWorldSeed;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::tests::wilderness_region;

static_assert(!entt::type_list_contains_v<AllComponents, SitePosition>);
static_assert(!entt::type_list_contains_v<AllComponents, WildernessVegetation>);
static_assert(!entt::type_list_contains_v<AllComponents, WildernessResourcePoint>);
static_assert(!entt::type_list_contains_v<AllComponents, WildernessEncounterPoint>);
static_assert(!entt::type_list_contains_v<AllComponents, WildernessPortal>);

template <typename Component>
[[nodiscard]] std::size_t component_count(const aetheria::zone::Zone& zone) {
    std::size_t result{};
    for ([[maybe_unused]] const auto entity : zone.reg.view<const Component>()) {
        ++result;
    }
    return result;
}

struct LiveCounts {
    std::size_t vegetation{};
    std::size_t resources{};
    std::size_t encounters{};
    std::size_t travelers{};
    std::size_t portals{};

    bool operator==(const LiveCounts&) const = default;
};

[[nodiscard]] LiveCounts live_counts(const aetheria::zone::Zone& zone) {
    return {component_count<WildernessVegetation>(zone),
            component_count<WildernessResourcePoint>(zone),
            component_count<WildernessEncounterPoint>(zone),
            component_count<WildernessTravelerPoint>(zone),
            component_count<WildernessPortal>(zone)};
}

TEST(WildernessLifecycle, PersistentLayerIsEmptyAndProceduralEntitiesAreNotSerialized) {
    auto tiles = wilderness_region();
    add_crossings(tiles);
    tiles.feature[tiles.index_of(kWildCenter)] =
        *test_ruleset().find_feature("feature.forest");
    auto materialized = aetheria::site::materialize_site_zone(
        tiles, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto& layers =
        std::get<aetheria::zone::SitePayload>(materialized.payload).layers;
    const auto before = live_counts(materialized);
    EXPECT_TRUE(layers.persistent.buildings.empty());
    EXPECT_TRUE(materialized.reg.view<const CityBuildState>().empty());
    EXPECT_TRUE(layers.procedural.valid_layout());
    EXPECT_GT(before.vegetation, 0U);
    EXPECT_GT(before.resources, 0U);
    EXPECT_GT(before.encounters, 0U);
    EXPECT_GT(before.travelers, 0U);
    EXPECT_GT(before.portals, 0U);

    const auto bytes = aetheria::serialize::encode_zone(materialized, test_ruleset());
    const auto loaded = aetheria::serialize::decode_zone(bytes, test_ruleset());
    const auto& loaded_layers =
        std::get<aetheria::zone::SitePayload>(loaded->payload).layers;
    EXPECT_TRUE(loaded_layers.persistent.buildings.empty());
    EXPECT_TRUE(loaded->reg.view<const CityBuildState>().empty());
    EXPECT_TRUE(loaded_layers.procedural.skeleton.ground.empty());
    EXPECT_EQ(live_counts(*loaded), LiveCounts{});
    std::cout << "wild_persistence persistent_buildings=0 serialized_procedural_entities=0"
              << " live_vegetation=" << before.vegetation
              << " live_resources=" << before.resources
              << " live_encounters=" << before.encounters
              << " live_travelers=" << before.travelers
              << " live_portals=" << before.portals << '\n';
}

TEST(WildernessLifecycle, ColdRematerializeRecomputesAfterProceduralCorruption) {
    TemporaryDirectory directory;
    auto tiles = wilderness_region();
    add_crossings(tiles);
    tiles.feature[tiles.index_of(kWildCenter)] =
        *test_ruleset().find_feature("feature.forest");
    aetheria::zone::FileZoneStore store{directory.path(), test_ruleset()};
    store.save(aetheria::zone::Zone{aetheria::zone::kRootZone});
    auto initial = aetheria::site::materialize_site_zone(
        tiles, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto expected = live_counts(initial);
    const auto key = initial.key;
    store.save(initial);
    auto& state = tiles.site[tiles.index_of(kWildCenter)];
    state.lod = aetheria::zone::LodLevel::Absent;
    state.has_live_site = false;
    store.write_manifest({.world_seed = kWildWorldSeed});
    aetheria::zone::ZoneManager manager{store};

    const auto first = aetheria::site::rematerialize_site_zone(
        manager, tiles, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    ASSERT_TRUE(manager.with(first, [&](aetheria::zone::Zone& zone) {
        EXPECT_EQ(live_counts(zone), expected);
        auto& procedural =
            std::get<aetheria::zone::SitePayload>(zone.payload).layers.procedural;
        procedural.skeleton.ground.clear();
        std::vector<entt::entity> entities;
        for (const auto entity : zone.reg.view<SitePosition>()) {
            entities.push_back(entity);
        }
        for (const auto entity : entities) {
            zone.reg.destroy(entity);
        }
        EXPECT_EQ(live_counts(zone), LiveCounts{});
    }));
    aetheria::site::collapse_site_zone(manager, first, tiles, kWildCenter);
    ASSERT_FALSE(manager.get(key).has_value());

    const auto second = aetheria::site::rematerialize_site_zone(
        manager, tiles, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    ASSERT_TRUE(manager.with(second, [&](const aetheria::zone::Zone& zone) {
        const auto& layers = std::get<aetheria::zone::SitePayload>(zone.payload).layers;
        EXPECT_TRUE(layers.procedural.valid_layout());
        EXPECT_TRUE(layers.persistent.buildings.empty());
        EXPECT_EQ(live_counts(zone), expected);
    }));
    std::cout << "wild_rematerialize cold_runs=2 cache_corruption=1 recomputed=1"
              << " persistent_objects=0 procedural_entities="
              << (expected.vegetation + expected.resources + expected.encounters +
                  expected.travelers + expected.portals)
              << '\n';
}

}  // namespace
