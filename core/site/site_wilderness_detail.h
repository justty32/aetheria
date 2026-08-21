#pragma once

// 荒野 W1～W6 分段實作的內部共用介面。

#include "core/site/site_wilderness.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aetheria::site::wilderness_detail {

inline constexpr std::size_t kDirections = 4;

[[nodiscard]] constexpr std::size_t tile_index(SiteXY tile) noexcept {
    return static_cast<std::size_t>(tile.y) * kSiteWidth + tile.x;
}

[[nodiscard]] SiteXY boundary_tile(SiteBoundarySide side, std::uint8_t position) noexcept;
[[nodiscard]] std::vector<SiteXY> find_path(const SiteSkeleton& terrain, SiteXY start,
                                            SiteXY goal, bool avoid_water);
void generate_wilderness_terrain(WildernessSkeleton& result, const WildernessSlowVars& slow,
                                 std::uint64_t site_seed, const rules::Ruleset& ruleset);
void generate_wilderness_paths(WildernessSkeleton& result, const WildernessSlowVars& slow,
                               const rules::Ruleset& ruleset);
void generate_wilderness_content(WildernessSkeleton& result, const WildernessSlowVars& slow,
                                 std::uint64_t site_seed, const rules::Ruleset& ruleset);

}  // namespace aetheria::site::wilderness_detail
