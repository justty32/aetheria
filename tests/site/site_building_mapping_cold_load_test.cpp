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

TEST(SiteBuildingMapping, ColdFileLoadDerivesTheSameTypeAndWeightFromDefinitionId) {
    static_assert(aetheria::serialize::kSaveFormatVersion == 23);
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
