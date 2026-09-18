#pragma once

// 骨架遷移共用的物件佔地與合法位置搜尋介面。

#include "core/site/site_projection.h"

#include <array>
#include <optional>

namespace aetheria::site::detail {

struct Footprint {
    std::uint8_t width{1};
    std::uint8_t height{1};
};

[[nodiscard]] bool can_place(const SiteSkeleton& skeleton,
                             const std::array<std::uint8_t, kSiteTileCount>& occupied,
                             SiteXY origin, Footprint footprint) noexcept;

void occupy(std::array<std::uint8_t, kSiteTileCount>& occupied, SiteXY origin,
            Footprint footprint) noexcept;

[[nodiscard]] std::optional<SiteXY> nearest_legal_origin(
    const SiteSkeleton& skeleton,
    const std::array<std::uint8_t, kSiteTileCount>& occupied, SiteXY former,
    Footprint footprint) noexcept;

}  // namespace aetheria::site::detail
