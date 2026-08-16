#pragma once

#include "core/base/check.h"

#include <cstdint>

namespace aetheria::zone {

// ZoneKey 是 zone 的穩定位址與身分。
// 它是無擁有者的值型別。
// 值本身永不失效。
enum class ZoneKey : std::uint64_t {};

// ZoneLevel 是 ZoneKey 高四位的空間層級。
// 它是無擁有者的值型別。
// 值本身永不失效。
enum class ZoneLevel : std::uint8_t {
    Root = 0,
    Region = 1,
    Site = 2,
    Local = 3,
    Detached = 15,
};

inline constexpr ZoneKey kRootZone{0};

namespace detail {

inline constexpr std::uint64_t kLevelShift = 60;
inline constexpr std::uint64_t kLevelMask = UINT64_C(0xF000000000000000);
inline constexpr std::uint64_t kRegionShift = 44;
inline constexpr std::uint64_t kRegionMask = UINT64_C(0x0FFFF00000000000);
inline constexpr std::uint64_t kSiteXShift = 32;
inline constexpr std::uint64_t kSiteXMask = UINT64_C(0x00000FFF00000000);
inline constexpr std::uint64_t kSiteYShift = 20;
inline constexpr std::uint64_t kSiteYMask = UINT64_C(0x00000000FFF00000);
inline constexpr std::uint64_t kLocalXShift = 10;
inline constexpr std::uint64_t kLocalXMask = UINT64_C(0x00000000000FFC00);
inline constexpr std::uint64_t kLocalYMask = UINT64_C(0x00000000000003FF);
inline constexpr std::uint64_t kDetachedIdMask = UINT64_C(0x0FFFFFFFFFFFFFFF);

[[nodiscard]] constexpr std::uint64_t bits(ZoneKey key) noexcept {
    return static_cast<std::uint64_t>(key);
}

[[nodiscard]] constexpr std::uint64_t level_bits(ZoneLevel level) noexcept {
    return static_cast<std::uint64_t>(level) << kLevelShift;
}

}  // namespace detail

[[nodiscard]] constexpr std::uint64_t value_of(ZoneKey key) noexcept { return detail::bits(key); }

[[nodiscard]] constexpr std::uint8_t level_value_of(ZoneKey key) noexcept {
    return static_cast<std::uint8_t>((detail::bits(key) & detail::kLevelMask) >>
                                     detail::kLevelShift);
}

[[nodiscard]] constexpr ZoneLevel level_of(ZoneKey key) noexcept {
    return static_cast<ZoneLevel>(level_value_of(key));
}

[[nodiscard]] constexpr std::uint32_t region_id_of(ZoneKey key) noexcept {
    return static_cast<std::uint32_t>((detail::bits(key) & detail::kRegionMask) >>
                                      detail::kRegionShift);
}

[[nodiscard]] constexpr std::uint32_t site_x_of(ZoneKey key) noexcept {
    return static_cast<std::uint32_t>((detail::bits(key) & detail::kSiteXMask) >>
                                      detail::kSiteXShift);
}

[[nodiscard]] constexpr std::uint32_t site_y_of(ZoneKey key) noexcept {
    return static_cast<std::uint32_t>((detail::bits(key) & detail::kSiteYMask) >>
                                      detail::kSiteYShift);
}

[[nodiscard]] constexpr std::uint32_t local_x_of(ZoneKey key) noexcept {
    if (level_of(key) == ZoneLevel::Site) {
        return site_x_of(key);
    }
    return static_cast<std::uint32_t>((detail::bits(key) & detail::kLocalXMask) >>
                                      detail::kLocalXShift);
}

[[nodiscard]] constexpr std::uint32_t local_y_of(ZoneKey key) noexcept {
    if (level_of(key) == ZoneLevel::Site) {
        return site_y_of(key);
    }
    return static_cast<std::uint32_t>(detail::bits(key) & detail::kLocalYMask);
}

[[nodiscard]] constexpr std::uint64_t detached_id_of(ZoneKey key) noexcept {
    return detail::bits(key) & detail::kDetachedIdMask;
}

[[nodiscard]] constexpr ZoneKey parent_of(ZoneKey key) noexcept {
    switch (level_of(key)) {
    case ZoneLevel::Root:
    case ZoneLevel::Region:
    case ZoneLevel::Detached:
        return kRootZone;
    case ZoneLevel::Site:
        return ZoneKey{detail::level_bits(ZoneLevel::Region) |
                       (detail::bits(key) & detail::kRegionMask)};
    case ZoneLevel::Local:
        return ZoneKey{
            detail::level_bits(ZoneLevel::Site) |
            (detail::bits(key) & (detail::kRegionMask | detail::kSiteXMask | detail::kSiteYMask))};
    }
    AETH_CHECK(false && "reserved ZoneLevel has no parent");
}

[[nodiscard]] constexpr ZoneKey child_key(ZoneKey parent, std::uint32_t x,
                                          std::uint32_t y) noexcept {
    switch (level_of(parent)) {
    case ZoneLevel::Root:
        AETH_CHECK(x <= UINT16_MAX && y == 0);
        return ZoneKey{detail::level_bits(ZoneLevel::Region) |
                       (static_cast<std::uint64_t>(x) << detail::kRegionShift)};
    case ZoneLevel::Region:
        AETH_CHECK(x <= UINT16_C(0x0FFF) && y <= UINT16_C(0x0FFF));
        return ZoneKey{detail::level_bits(ZoneLevel::Site) |
                       (detail::bits(parent) & detail::kRegionMask) |
                       (static_cast<std::uint64_t>(x) << detail::kSiteXShift) |
                       (static_cast<std::uint64_t>(y) << detail::kSiteYShift)};
    case ZoneLevel::Site:
        AETH_CHECK(x <= UINT16_C(0x03FF) && y <= UINT16_C(0x03FF));
        return ZoneKey{detail::level_bits(ZoneLevel::Local) |
                       (detail::bits(parent) &
                        (detail::kRegionMask | detail::kSiteXMask | detail::kSiteYMask)) |
                       (static_cast<std::uint64_t>(x) << detail::kLocalXShift) |
                       static_cast<std::uint64_t>(y)};
    case ZoneLevel::Local:
    case ZoneLevel::Detached:
        AETH_CHECK(false && "zone cannot have a spatial child");
        break;
    }
    AETH_CHECK(false && "reserved ZoneLevel cannot have a child");
}

// DetachedZoneKeyAllocator 配發非空間 zone 的單調序號。
// manifest 擁有它；M0.5 測試直接持有 in-memory 實例。
// allocator 存活期間已配發的序號永不復用。
class DetachedZoneKeyAllocator {
public:
    constexpr DetachedZoneKeyAllocator() noexcept = default;
    explicit constexpr DetachedZoneKeyAllocator(std::uint64_t next_id) noexcept
        : next_id_{next_id} {
        AETH_CHECK(next_id_ > 0 && next_id_ <= detail::kDetachedIdMask);
    }

    [[nodiscard]] constexpr ZoneKey allocate() noexcept {
        AETH_CHECK(next_id_ > 0 && next_id_ <= detail::kDetachedIdMask);
        const auto id = next_id_++;
        return ZoneKey{detail::level_bits(ZoneLevel::Detached) | id};
    }

    [[nodiscard]] constexpr std::uint64_t next_id() const noexcept { return next_id_; }

private:
    std::uint64_t next_id_{1};
};

}  // namespace aetheria::zone
