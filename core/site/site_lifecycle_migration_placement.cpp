// 骨架遷移的佔地檢查、佔用標記與最近合法位置搜尋。

#include "core/site/site_lifecycle_migration_detail.h"

#include <limits>

namespace aetheria::site::detail {

[[nodiscard]] bool can_place(const SiteSkeleton& skeleton,
                             const std::array<std::uint8_t, kSiteTileCount>& occupied,
                             SiteXY origin, Footprint footprint) noexcept {
    const auto x_end = static_cast<std::uint32_t>(origin.x) + footprint.width;
    const auto y_end = static_cast<std::uint32_t>(origin.y) + footprint.height;
    if (!skeleton.valid_layout() || x_end > kSiteWidth || y_end > kSiteHeight) {
        return false;
    }
    for (std::uint32_t y = origin.y; y < y_end; ++y) {
        for (std::uint32_t x = origin.x; x < x_end; ++x) {
            const auto index = static_cast<std::size_t>(y) * kSiteWidth + x;
            if (skeleton.buildable[index] == 0 || occupied[index] != 0) {
                return false;
            }
        }
    }
    return true;
}

void occupy(std::array<std::uint8_t, kSiteTileCount>& occupied, SiteXY origin,
            Footprint footprint) noexcept {
    for (std::uint32_t y = origin.y;
         y < static_cast<std::uint32_t>(origin.y) + footprint.height; ++y) {
        for (std::uint32_t x = origin.x;
             x < static_cast<std::uint32_t>(origin.x) + footprint.width; ++x) {
            occupied[static_cast<std::size_t>(y) * kSiteWidth + x] = UINT8_C(1);
        }
    }
}

[[nodiscard]] std::optional<SiteXY> nearest_legal_origin(
    const SiteSkeleton& skeleton,
    const std::array<std::uint8_t, kSiteTileCount>& occupied, SiteXY former,
    Footprint footprint) noexcept {
    std::optional<SiteXY> best;
    std::uint32_t best_distance = std::numeric_limits<std::uint32_t>::max();
    for (std::uint16_t y = 0; y < kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < kSiteWidth; ++x) {
            const SiteXY candidate{x, y};
            if (!can_place(skeleton, occupied, candidate, footprint)) {
                continue;
            }
            const auto x_distance = former.x > x ? former.x - x : x - former.x;
            const auto y_distance = former.y > y ? former.y - y : y - former.y;
            const auto distance = static_cast<std::uint32_t>(x_distance) + y_distance;
            if (distance < best_distance) {
                best = candidate;
                best_distance = distance;
            }
        }
    }
    return best;
}

}  // namespace aetheria::site::detail
