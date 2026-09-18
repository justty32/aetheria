#pragma once

// 城牆環與護城河生成的內部共用介面。

#include "core/site/site_fill_detail.h"

#include <array>
#include <cstdint>

namespace aetheria::site::fill_detail::fortifications_detail {

struct Bounds {
    std::uint16_t x0{};
    std::uint16_t y0{};
    std::uint16_t x1{};
    std::uint16_t y1{};

    constexpr bool operator==(const Bounds&) const noexcept = default;
};

[[nodiscard]] constexpr std::array<std::int16_t, 2> offset_for(SiteBoundarySide side) noexcept {
    switch (side) {
    case SiteBoundarySide::North:
        return {0, -1};
    case SiteBoundarySide::East:
        return {1, 0};
    case SiteBoundarySide::South:
        return {0, 1};
    case SiteBoundarySide::West:
        return {-1, 0};
    }
    return {};
}

void add_ring(SiteProceduralLayer& layer, Bounds bounds, const SiteFastVars& fast,
              const rules::Ruleset& ruleset);
void add_moat(SiteProceduralLayer& layer, Bounds wall_bounds, const rules::Ruleset& ruleset);

}  // namespace aetheria::site::fill_detail::fortifications_detail
