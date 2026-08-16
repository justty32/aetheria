#include "core/zone/zone_key.h"
#include "tests/zone/zone_test_support.h"

#include <cstdint>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::kLocal;
using aetheria::tests::kRegion;
using aetheria::tests::kSite;
using aetheria::zone::child_key;
using aetheria::zone::detached_id_of;
using aetheria::zone::DetachedZoneKeyAllocator;
using aetheria::zone::kRootZone;
using aetheria::zone::level_of;
using aetheria::zone::level_value_of;
using aetheria::zone::local_x_of;
using aetheria::zone::local_y_of;
using aetheria::zone::parent_of;
using aetheria::zone::region_id_of;
using aetheria::zone::site_x_of;
using aetheria::zone::site_y_of;
using aetheria::zone::ZoneLevel;

static_assert(level_of(kRootZone) == ZoneLevel::Root);
static_assert(level_of(kRegion) == ZoneLevel::Region);
static_assert(level_of(kSite) == ZoneLevel::Site);
static_assert(level_of(kLocal) == ZoneLevel::Local);
static_assert(region_id_of(kRegion) == UINT16_C(0xA55A));
static_assert(site_x_of(kSite) == UINT16_C(0x0ABC));
static_assert(site_y_of(kSite) == UINT16_C(0x0123));
static_assert(local_x_of(kLocal) == UINT16_C(0x0321));
static_assert(local_y_of(kLocal) == UINT16_C(0x0012));
static_assert(parent_of(kRegion) == kRootZone);
static_assert(parent_of(kSite) == kRegion);
static_assert(parent_of(kLocal) == kSite);

TEST(ZoneKey, RoundTripsEveryCoordinateValueAtEachLevel) {
    for (std::uint32_t region_id = 0; region_id <= UINT16_MAX; ++region_id) {
        const auto region = child_key(kRootZone, region_id, 0);
        ASSERT_EQ(parent_of(region), kRootZone);
        ASSERT_EQ(region_id_of(region), region_id);
    }

    constexpr auto region = child_key(kRootZone, UINT16_MAX, 0);
    for (std::uint32_t x = 0; x <= UINT16_C(0x0FFF); ++x) {
        for (std::uint32_t y = 0; y <= UINT16_C(0x0FFF); ++y) {
            const auto site = child_key(region, x, y);
            ASSERT_EQ(parent_of(site), region);
            ASSERT_EQ(local_x_of(site), x);
            ASSERT_EQ(local_y_of(site), y);
        }
    }

    constexpr auto site = child_key(region, UINT16_C(0x0FFF), UINT16_C(0x0FFF));
    for (std::uint32_t x = 0; x <= UINT16_C(0x03FF); ++x) {
        for (std::uint32_t y = 0; y <= UINT16_C(0x03FF); ++y) {
            const auto local = child_key(site, x, y);
            ASSERT_EQ(parent_of(local), site);
            ASSERT_EQ(local_x_of(local), x);
            ASSERT_EQ(local_y_of(local), y);
        }
    }
}

TEST(ZoneKey, CoversEveryDefinedLevelAndFieldBoundary) {
    DetachedZoneKeyAllocator allocator;
    const auto detached = allocator.allocate();

    EXPECT_EQ(level_value_of(kRootZone), 0);
    EXPECT_EQ(level_value_of(child_key(kRootZone, 0, 0)), 1);
    EXPECT_EQ(level_value_of(child_key(child_key(kRootZone, 0, 0), 0, 0)), 2);
    EXPECT_EQ(level_value_of(child_key(child_key(child_key(kRootZone, 0, 0), 0, 0), 0, 0)), 3);
    EXPECT_EQ(level_value_of(detached), 15);
    EXPECT_EQ(detached_id_of(detached), 1);

    const auto max_region = child_key(kRootZone, UINT16_MAX, 0);
    const auto max_site = child_key(max_region, UINT16_C(0x0FFF), UINT16_C(0x0FFF));
    const auto max_local = child_key(max_site, UINT16_C(0x03FF), UINT16_C(0x03FF));
    EXPECT_EQ(region_id_of(max_local), UINT16_MAX);
    EXPECT_EQ(site_x_of(max_local), UINT16_C(0x0FFF));
    EXPECT_EQ(site_y_of(max_local), UINT16_C(0x0FFF));
    EXPECT_EQ(local_x_of(max_local), UINT16_C(0x03FF));
    EXPECT_EQ(local_y_of(max_local), UINT16_C(0x03FF));

    DetachedZoneKeyAllocator maximum_allocator{UINT64_C(0x0FFFFFFFFFFFFFFF)};
    const auto maximum_detached = maximum_allocator.allocate();
    EXPECT_EQ(detached_id_of(maximum_detached), UINT64_C(0x0FFFFFFFFFFFFFFF));
    EXPECT_DEATH(static_cast<void>(maximum_allocator.allocate()), "AETH_CHECK failed: next_id_");
    EXPECT_DEATH(DetachedZoneKeyAllocator{0}, "AETH_CHECK failed: next_id_");
}

TEST(ZoneKey, RootAndDetachedHaveRootParent) {
    DetachedZoneKeyAllocator allocator;
    EXPECT_EQ(parent_of(kRootZone), kRootZone);
    EXPECT_EQ(parent_of(allocator.allocate()), kRootZone);
}

}  // namespace
