#include "core/zone/zone.h"
#include "core/zone/zone_key.h"
#include "tests/zone/zone_test_support.h"

#include <concepts>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::entity_count;
using aetheria::tests::kLocal;
using aetheria::tests::kRegion;
using aetheria::tests::kSite;
using aetheria::zone::child_key;
using aetheria::zone::DetachedZoneKeyAllocator;
using aetheria::zone::kRootZone;
using aetheria::zone::value_of;
using aetheria::zone::Zone;
using aetheria::zone::ZoneMeta;

static_assert(!std::is_copy_constructible_v<Zone>);
static_assert(std::is_move_constructible_v<Zone>);
static_assert(requires(aetheria::zone::SitePayload payload) { payload.layers; });
static_assert(requires(aetheria::zone::LocalPayload payload) { payload.tiles; });

TEST(ZonePersistence, NewZoneStartsWithMatchingPlaceholder) {
    const auto key = child_key(kRootZone, UINT16_C(0x4321), 0);
    const Zone zone{key};
    const auto meta = zone.reg.view<const ZoneMeta>();

    ASSERT_EQ(meta.size(), 1U);
    EXPECT_EQ(meta.get<ZoneMeta>(*meta.begin()).zone_key, value_of(key));
    EXPECT_EQ(entity_count(zone), 1U);
}

TEST(SpatialPayload, DefaultAlternativeMatchesEveryDefinedZoneLevel) {
    DetachedZoneKeyAllocator allocator;
    const Zone root{kRootZone};
    const Zone region{kRegion};
    const Zone site{kSite};
    const Zone local{kLocal};
    const Zone detached{allocator.allocate()};

    EXPECT_TRUE(std::holds_alternative<std::monostate>(root.payload));
    EXPECT_TRUE(std::holds_alternative<aetheria::zone::RegionPayload>(region.payload));
    EXPECT_TRUE(std::holds_alternative<aetheria::zone::SitePayload>(site.payload));
    EXPECT_TRUE(std::holds_alternative<aetheria::zone::LocalPayload>(local.payload));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(detached.payload));
}

TEST(SpatialPayload, ConstructorRejectsLevelMismatchAndTilesOnRootOrDetached) {
    DetachedZoneKeyAllocator allocator;
    const auto detached = allocator.allocate();

    EXPECT_THROW(Zone(kSite, aetheria::zone::RegionPayload{}), std::invalid_argument);
    EXPECT_THROW(Zone(kRootZone, aetheria::zone::RegionPayload{}), std::invalid_argument);
    EXPECT_THROW(Zone(detached, aetheria::zone::RegionPayload{}), std::invalid_argument);
}

}  // namespace
