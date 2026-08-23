#include "core/serialize/zone_codec.h"
#include "core/runtime/playable_session.h"
#include "core/site/site_reduction.h"
#include "core/zone/file_zone_store.h"
#include "tests/site/site_build_loop_test_support.h"
#include "tests/zone/zone_test_support.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string_view>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::PersistentBuildingType;
using aetheria::site::BuildingReductionWeight;
using aetheria::site::BuildingState;
using aetheria::site::PersistentBuilding;
using aetheria::tests::build_fixture;
using aetheria::tests::kBuildCoordinate;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::world::DevelopmentLevelReduction;
using aetheria::world::PopulationReduction;

struct ExpectedMapping {
    std::string_view definition_id;
    PersistentBuildingType type;
    BuildingReductionWeight weight;
};

constexpr std::array kExpectedMappings{
    ExpectedMapping{"city.house", PersistentBuildingType::Residence, {0, 2}},
    ExpectedMapping{"city.farm", PersistentBuildingType::Farm, {0, 1}},
    ExpectedMapping{"city.mine", PersistentBuildingType::Mine, {0, 2}},
    ExpectedMapping{"city.workshop", PersistentBuildingType::Workshop, {0, 2}},
    ExpectedMapping{"city.square", PersistentBuildingType::CivicSquare, {0, 1}},
};

TEST(SiteBuildingMapping, EveryCityBuildingDefHasTheExpectedPersistentTypeAndWeight) {
    const auto& ruleset = test_ruleset();
    ASSERT_EQ(ruleset.city_buildings().size(), kExpectedMappings.size());
    for (const auto& expected : kExpectedMappings) {
        const auto id = ruleset.find_city_building(expected.definition_id);
        ASSERT_TRUE(id.has_value()) << expected.definition_id;
        const auto* definition = ruleset.city_building(*id);
        ASSERT_NE(definition, nullptr);
        EXPECT_EQ(definition->persistent_type, expected.type) << expected.definition_id;
        EXPECT_EQ(aetheria::site::building_reduction_weight(definition->persistent_type),
                  expected.weight)
            << expected.definition_id;
        std::cout << "building_mapping id=" << expected.definition_id << " type="
                  << aetheria::rules::persistent_building_type_name(expected.type)
                  << " population_weight=" << expected.weight.population
                  << " development_weight=" << expected.weight.development << '\n';
    }
}

TEST(SiteBuildingMapping, HouseAndSettlementHallHaveDifferentObservableRegionEffects) {
    auto fixture = build_fixture();
    auto& tiles =
        std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    auto& persistent =
        std::get<aetheria::zone::SitePayload>(fixture.site.payload).layers.persistent;
    ASSERT_EQ(persistent.buildings.size(), 1U);
    persistent.buildings.front().state = BuildingState::Idle;
    aetheria::site::reduce_live_site_xun(tiles, kBuildCoordinate, fixture.site,
                                         test_ruleset());
    const auto house_population_before =
        tiles.reduction_value<PopulationReduction>(kBuildCoordinate);
    const auto house_development_before =
        tiles.reduction_value<DevelopmentLevelReduction>(kBuildCoordinate);
    const auto raw_population_before = *aetheria::site::ReductionTable::reduce(
                                            std::get<aetheria::zone::SitePayload>(
                                                fixture.site.payload)
                                                .layers)
                                            .value<PopulationReduction>();

    aetheria::site::start_construction(fixture.site, "city.house", {10, 10},
                                       test_ruleset());
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    aetheria::site::SiteTurnPipeline pipeline{test_ruleset(), store};
    const auto report = pipeline.advance_hours(fixture.site, fixture.region, 0,
                                               kBuildCoordinate, 24);
    ASSERT_EQ(report.constructions_completed, 1U);
    const auto house_population_after =
        tiles.reduction_value<PopulationReduction>(kBuildCoordinate);
    const auto house_development_after =
        tiles.reduction_value<DevelopmentLevelReduction>(kBuildCoordinate);
    const auto& layers =
        std::get<aetheria::zone::SitePayload>(fixture.site.payload).layers;
    const auto raw_population_after =
        *aetheria::site::ReductionTable::reduce(layers).value<PopulationReduction>();

    aetheria::site::SiteLayers empty;
    const auto hall_population_before =
        *aetheria::site::ReductionTable::reduce(empty).value<PopulationReduction>();
    const auto hall_development_before =
        *aetheria::site::ReductionTable::reduce(empty).value<DevelopmentLevelReduction>();
    empty.persistent.buildings.push_back(
        PersistentBuilding{{1, 1}, PersistentBuildingType::SettlementHall,
                           BuildingState::Active});
    const auto hall_delta = aetheria::site::ReductionTable::reduce(empty);
    const auto hall_population_after = *hall_delta.value<PopulationReduction>();
    const auto hall_development_after = *hall_delta.value<DevelopmentLevelReduction>();

    EXPECT_EQ(layers.persistent.buildings.size(), 1U);
    EXPECT_EQ(raw_population_before, 75U);
    EXPECT_EQ(raw_population_after, 75U);
    EXPECT_EQ(house_population_after - house_population_before, 0U);
    EXPECT_EQ(house_development_after - house_development_before, 2U);
    EXPECT_EQ(hall_population_after - hall_population_before, 100U);
    EXPECT_EQ(hall_development_after - hall_development_before, 1U);
    EXPECT_NE(house_population_after - house_population_before,
              hall_population_after - hall_population_before);
    EXPECT_NE(house_development_after - house_development_before,
              hall_development_after - hall_development_before);

    aetheria::runtime::PlayableSession playable{515151, 51,
                                                AETHERIA_SOURCE_DIR "/data"};
    const auto runtime_before = playable.coverage_summary();
    playable.enter_site();
    playable.build_city();
    const auto runtime_after = playable.coverage_summary();
    EXPECT_EQ(runtime_before.persistent_buildings, 0U);
    EXPECT_EQ(runtime_after.persistent_buildings, 1U);
    EXPECT_EQ(runtime_before.persistent_population, 0U);
    EXPECT_EQ(runtime_after.persistent_population, 100U);
    std::cout << "building_region_effect house_population=" << house_population_before
              << "->" << house_population_after << " house_development="
              << house_development_before << "->" << house_development_after
              << " hall_population=" << hall_population_before << "->"
              << hall_population_after << " hall_development=" << hall_development_before
              << "->" << hall_development_after << " marker_count_before=1 marker_count_after="
              << layers.persistent.buildings.size() << " raw_population="
              << raw_population_before << "->" << raw_population_after << '\n';
    std::cout << "marker_runtime persistent_buildings="
              << runtime_before.persistent_buildings << "->"
              << runtime_after.persistent_buildings << " persistent_population="
              << runtime_before.persistent_population << "->"
              << runtime_after.persistent_population << '\n';
}

TEST(SiteBuildingMapping, ColdFileLoadDerivesTheSameTypeAndWeightFromDefinitionId) {
    static_assert(aetheria::serialize::kSaveFormatVersion == 21);
    auto fixture = build_fixture();
    aetheria::site::start_construction(fixture.site, "city.house", {10, 10},
                                       test_ruleset());
    aetheria::zone::InMemoryZoneStore memory_store{test_ruleset()};
    aetheria::site::SiteTurnPipeline pipeline{test_ruleset(), memory_store};
    ASSERT_EQ(pipeline.advance_hours(fixture.site, fixture.region, 0, kBuildCoordinate, 24)
                  .constructions_completed,
              1U);
    const auto& source_building = aetheria::site::city_build_state(fixture.site).buildings.front();
    const auto source_definition_id = source_building.definition_id;
    const auto source_id = test_ruleset().find_city_building(source_building.definition_id);
    ASSERT_TRUE(source_id.has_value());
    const auto source_type = test_ruleset().city_building(*source_id)->persistent_type;
    const auto source_weight = aetheria::site::building_reduction_weight(source_type);
    const auto source_delta =
        aetheria::site::ReductionTable::reduce(fixture.site, test_ruleset());

    TemporaryDirectory directory;
    aetheria::zone::FileZoneStore store{directory.path(), test_ruleset()};
    store.save(aetheria::zone::Zone{aetheria::zone::kRootZone});
    auto source = std::make_unique<aetheria::zone::Zone>(std::move(fixture.site));
    store.save(*source);
    store.write_manifest(aetheria::zone::SaveManifest{});
    const auto site_key = source->key;
    source.reset();
    ASSERT_EQ(source, nullptr);

    aetheria::zone::FileZoneStore cold_store{directory.path(), test_ruleset()};
    const auto loaded = cold_store.load(site_key);
    ASSERT_NE(loaded, nullptr);
    const auto& loaded_state = aetheria::site::city_build_state(*loaded);
    ASSERT_EQ(loaded_state.buildings.size(), 1U);
    const auto loaded_id =
        test_ruleset().find_city_building(loaded_state.buildings.front().definition_id);
    ASSERT_TRUE(loaded_id.has_value());
    const auto loaded_type = test_ruleset().city_building(*loaded_id)->persistent_type;
    const auto loaded_weight = aetheria::site::building_reduction_weight(loaded_type);
    const auto loaded_delta =
        aetheria::site::ReductionTable::reduce(*loaded, test_ruleset());

    EXPECT_EQ(loaded_state.buildings.front().definition_id, source_definition_id);
    EXPECT_EQ(loaded_type, source_type);
    EXPECT_EQ(loaded_weight, source_weight);
    EXPECT_EQ(loaded_delta.value<PopulationReduction>(),
              source_delta.value<PopulationReduction>());
    EXPECT_EQ(loaded_delta.value<DevelopmentLevelReduction>(),
              source_delta.value<DevelopmentLevelReduction>());
    std::cout << "building_mapping_cold_load source_destroyed=1 definition_id="
              << loaded_state.buildings.front().definition_id << " type="
              << aetheria::rules::persistent_building_type_name(loaded_type)
              << " population_weight=" << loaded_weight.population
              << " development_weight=" << loaded_weight.development << " reduction_population="
              << *loaded_delta.value<PopulationReduction>() << " reduction_development="
              << *loaded_delta.value<DevelopmentLevelReduction>() << '\n';
}

}  // namespace
