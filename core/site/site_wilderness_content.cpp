#include "core/site/site_wilderness_detail.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace aetheria::site::wilderness_detail {
namespace {

constexpr std::uint64_t kVegetationSalt = UINT64_C(0x17B4E06D92C53A8F);
constexpr std::uint64_t kPortalSalt = UINT64_C(0x93D6215AF84C07BE);
constexpr std::uint64_t kRuinSalt = UINT64_C(0x6AC018F4D35E927B);

[[nodiscard]] bool contains(const std::vector<SiteXY>& points, SiteXY value) {
    return std::ranges::find(points, value) != points.end();
}

[[nodiscard]] SiteXY pick_passable(const SiteSkeleton& terrain, std::uint64_t seed,
                                   const std::vector<SiteXY>& used) {
    const auto start = static_cast<std::size_t>(worldgen::splitmix64(seed) % kSiteTileCount);
    for (std::size_t offset = 0; offset < kSiteTileCount; ++offset) {
        const auto index = (start + offset * 977U) % kSiteTileCount;
        const SiteXY candidate{static_cast<std::uint16_t>(index % kSiteWidth),
                               static_cast<std::uint16_t>(index / kSiteWidth)};
        if (terrain.buildable[index] != 0 && terrain.roads[index] == 0 &&
            !contains(used, candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error{"荒野 W4/W6 找不到可通行的程序物件位置"};
}

void scatter_vegetation(WildernessSkeleton& result, std::uint64_t site_seed,
                        std::uint8_t density, std::uint8_t cell_extent) {
    for (std::uint16_t cell_y = 0; cell_y < kSiteHeight; cell_y += cell_extent) {
        for (std::uint16_t cell_x = 0; cell_x < kSiteWidth; cell_x += cell_extent) {
            const auto cell = static_cast<std::uint64_t>(cell_y) * kSiteWidth + cell_x;
            const auto seed = worldgen::splitmix64(site_seed ^ kVegetationSalt ^ cell);
            if (seed % 100U >= density) {
                continue;
            }
            const auto width = std::min<std::uint16_t>(cell_extent, kSiteWidth - cell_x);
            const auto height = std::min<std::uint16_t>(cell_extent, kSiteHeight - cell_y);
            const SiteXY tile{static_cast<std::uint16_t>(cell_x + seed % width),
                              static_cast<std::uint16_t>(cell_y + (seed >> 16U) % height)};
            const auto index = tile_index(tile);
            if (result.terrain.buildable[index] != 0 && result.terrain.roads[index] == 0) {
                result.vegetation.push_back(tile);
            }
        }
    }
}

void place_portals(WildernessSkeleton& result, std::uint64_t site_seed, std::uint8_t count) {
    for (std::uint8_t index = 0; index < count; ++index) {
        result.portals.push_back(
            pick_passable(result.terrain, site_seed ^ kPortalSalt ^ index, result.portals));
    }
}

void retain_ruin_blocks(WildernessSkeleton& result, const WildernessSlowVars& slow,
                        std::uint64_t site_seed, const rules::Ruleset& ruleset) {
    const auto city = build_site_skeleton(slow.local, site_seed ^ kRuinSalt, ruleset);
    const auto& config = ruleset.wilderness_generation_rules();
    const auto span = static_cast<std::uint16_t>(config.ruin_keep_max_percent -
                                                 config.ruin_keep_min_percent + 1U);
    const auto percent = static_cast<std::uint16_t>(
        config.ruin_keep_min_percent + worldgen::splitmix64(site_seed ^ kRuinSalt) % span);
    const auto keep = std::max<std::size_t>(1U, city.blocks.size() * percent / 100U);
    for (std::size_t index = 0; index < city.blocks.size(); ++index) {
        const auto remaining = city.blocks.size() - index;
        const auto needed = keep - result.ruin_structures.size();
        const auto draw = worldgen::splitmix64(site_seed ^ kRuinSalt ^ index);
        if (needed >= remaining || draw % remaining < needed) {
            result.ruin_structures.push_back(city.blocks[index]);
        }
    }
}

}  // namespace

void generate_wilderness_content(WildernessSkeleton& result, const WildernessSlowVars& slow,
                                 std::uint64_t site_seed, const rules::Ruleset& ruleset) {
    const auto* terrain = ruleset.terrain(slow.local.base);
    const auto* relief = ruleset.relief(slow.local.relief);
    const auto* feature = ruleset.feature(slow.local.feature);
    if (terrain == nullptr || relief == nullptr || feature == nullptr) {
        throw std::runtime_error{"荒野 W4/W6 引用不存在的 def"};
    }
    const auto& config = ruleset.wilderness_generation_rules();
    const bool sea = (terrain->flags & rules::kTerrainWaterFlag) != 0;
    const bool forest = (feature->flags & rules::kFeatureForestFlag) != 0;
    const bool ruin = (feature->flags & rules::kFeatureRuinFlag) != 0;
    if (!sea) {
        scatter_vegetation(result, site_seed,
                           forest ? config.forest_vegetation_percent
                                  : config.sparse_vegetation_percent,
                           config.jitter_cell_extent);
    }
    if (ruin) {
        retain_ruin_blocks(result, slow, site_seed, ruleset);
    }
    const auto portal_count = ruin ? config.ruin_portals
                                   : (relief->move_cost >= 4 ? config.mountain_portals
                                                            : config.wilderness_portals);
    if (!sea) {
        place_portals(result, site_seed, portal_count);
    }
}

}  // namespace aetheria::site::wilderness_detail
