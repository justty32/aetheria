#include "core/local/local_materialize.h"
#include "core/serialize/zone_codec.h"
#include "tests/local/local_test_support.h"

#include <concepts>
#include <cstdint>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::kLocalCenter;
using aetheria::tests::kLocalSiteSeed;
using aetheria::tests::open_site_layer;
using aetheria::tests::test_ruleset;

[[nodiscard]] aetheria::zone::ZoneKey sample_site_key() {
    const auto region = aetheria::zone::child_key(aetheria::zone::kRootZone, 7, 0);
    return aetheria::zone::child_key(region, 4, 9);
}

TEST(LocalMaterialize, PayloadHasAllFiveTileFieldsAndCorrectKey) {
    const auto parent = open_site_layer();
    const auto site_key = sample_site_key();
    const auto feature = *test_ruleset().find_feature("feature.none");
    const auto local = aetheria::local::materialize_local_zone(
        site_key, parent, kLocalCenter, kLocalSiteSeed, feature, test_ruleset());
    EXPECT_EQ(aetheria::zone::parent_of(local.key), site_key);
    EXPECT_EQ(aetheria::zone::local_x_of(local.key), kLocalCenter.x);
    EXPECT_EQ(aetheria::zone::local_y_of(local.key), kLocalCenter.y);
    EXPECT_EQ(local.lod, aetheria::zone::LodLevel::Full);
    EXPECT_TRUE(std::get<aetheria::zone::LocalPayload>(local.payload).tiles.valid_layout());
}

TEST(LocalMaterialize, LoadAndRematerializeAreDistinctEntrypoints) {
    const auto parent = open_site_layer();
    const auto site_key = sample_site_key();
    const auto feature = *test_ruleset().find_feature("feature.none");
    auto materialized = aetheria::local::materialize_local_zone(
        site_key, parent, kLocalCenter, kLocalSiteSeed, feature, test_ruleset());
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(materialized);
    aetheria::zone::ZoneManager manager{store};

    const auto loaded = aetheria::local::load_local_zone(manager, materialized.key);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(manager.with(*loaded, [](const aetheria::zone::Zone& zone) {
        EXPECT_TRUE(std::get<aetheria::zone::LocalPayload>(zone.payload).tiles.empty());
    }));
    ASSERT_TRUE(manager.unload(materialized.key));

    const auto rematerialized = aetheria::local::rematerialize_local_zone(
        manager, site_key, parent, kLocalCenter, kLocalSiteSeed, feature, test_ruleset());
    ASSERT_TRUE(manager.with(rematerialized, [](const aetheria::zone::Zone& zone) {
        EXPECT_TRUE(std::get<aetheria::zone::LocalPayload>(zone.payload).tiles.valid_layout());
    }));
}

}  // namespace
