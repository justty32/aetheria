#include "core/site/site_wilderness.h"

#include "core/site/site_wilderness_detail.h"
#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace aetheria::site {
namespace {

constexpr std::uint64_t kResourceSalt = UINT64_C(0xA7D31C5E920BF846);
constexpr std::uint64_t kEncounterSalt = UINT64_C(0x2E8B64F190C735AD);
constexpr std::uint64_t kTravelerSalt = UINT64_C(0xD406B83A71E529CF);

[[nodiscard]] bool in_bounds(SiteXY tile) noexcept {
    return tile.x < kSiteWidth && tile.y < kSiteHeight;
}

template <typename Predicate>
[[nodiscard]] std::vector<SiteXY> pick_points(const SiteSkeleton& terrain, std::uint8_t count,
                                              std::uint64_t seed, Predicate eligible) {
    std::vector<SiteXY> result;
    result.reserve(count);
    const auto start = static_cast<std::size_t>(worldgen::splitmix64(seed) % kSiteTileCount);
    for (std::size_t offset = 0; offset < kSiteTileCount && result.size() < count; ++offset) {
        const auto index = (start + offset * 977U) % kSiteTileCount;
        const SiteXY tile{static_cast<std::uint16_t>(index % kSiteWidth),
                          static_cast<std::uint16_t>(index / kSiteWidth)};
        if (eligible(terrain, index) && std::ranges::find(result, tile) == result.end()) {
            result.push_back(tile);
        }
    }
    if (result.size() != count) {
        throw std::runtime_error{"荒野 W5 找不到足夠的程序點位置"};
    }
    return result;
}

}  // namespace

bool WildernessSkeleton::valid_layout() const noexcept {
    if (!terrain.valid_layout() ||
        !std::ranges::all_of(vegetation, in_bounds) ||
        !std::ranges::all_of(portals, in_bounds)) {
        return false;
    }
    return std::ranges::all_of(ruin_structures, [](const SiteBlock& block) {
        return block.width != 0 && block.height != 0 && in_bounds(block.origin) &&
               static_cast<std::uint32_t>(block.origin.x) + block.width <= kSiteWidth &&
               static_cast<std::uint32_t>(block.origin.y) + block.height <= kSiteHeight;
    });
}

bool WildernessSite::valid_layout() const noexcept {
    return skeleton.valid_layout() &&
           std::ranges::all_of(population.resource_points, in_bounds) &&
           std::ranges::all_of(population.encounter_points, in_bounds) &&
           std::ranges::all_of(population.traveler_points, in_bounds);
}

WildernessSkeleton build_wilderness_skeleton(const WildernessSlowVars& slow,
                                             std::uint64_t site_seed,
                                             const rules::Ruleset& ruleset) {
    if (!ruleset.wilderness_generation_rules().loaded ||
        ruleset.terrain(slow.local.base) == nullptr ||
        ruleset.relief(slow.local.relief) == nullptr ||
        ruleset.feature(slow.local.feature) == nullptr) {
        throw std::runtime_error{"荒野骨架含無效慢變數或缺少資料規則"};
    }
    WildernessSkeleton result;
    result.source_base = slow.local.base;
    result.source_relief = slow.local.relief;
    result.source_feature = slow.local.feature;
    wilderness_detail::generate_wilderness_terrain(result, slow, site_seed, ruleset);
    wilderness_detail::generate_wilderness_paths(result, slow, ruleset);
    wilderness_detail::generate_wilderness_content(result, slow, site_seed, ruleset);
    if (!result.valid_layout()) {
        throw std::logic_error{"荒野 W1～W4/W6 產生無效版面"};
    }
    return result;
}

WildernessSite populate_wilderness(WildernessSkeleton skeleton, const SiteFastVars& fast,
                                   std::uint64_t site_seed, const rules::Ruleset& ruleset) {
    if (!skeleton.valid_layout()) {
        throw std::runtime_error{"荒野 W5 拒絕無效骨架"};
    }
    const auto* terrain = ruleset.terrain(skeleton.source_base);
    const auto* feature = ruleset.feature(skeleton.source_feature);
    if (terrain == nullptr || feature == nullptr) {
        throw std::runtime_error{"荒野 W5 引用不存在的 def"};
    }
    const auto& config = ruleset.wilderness_generation_rules();
    const bool sea = (terrain->flags & rules::kTerrainWaterFlag) != 0;
    const bool mine = (feature->flags & rules::kFeatureMineFlag) != 0;
    const bool unowned = static_cast<std::uint16_t>(fast.owner) == 0;
    WildernessPopulation population;
    if (!sea) {
        const auto resource_count = mine ? config.mine_resource_points : config.base_resource_points;
        population.resource_points = pick_points(
            skeleton.terrain, resource_count, site_seed ^ kResourceSalt,
            [](const SiteSkeleton& value, std::size_t index) {
                return value.buildable[index] != 0 && value.roads[index] == 0;
            });
    }
    population.encounter_points = pick_points(
        skeleton.terrain,
        unowned ? config.unowned_encounter_points : config.owned_encounter_points,
        site_seed ^ kEncounterSalt, [sea](const SiteSkeleton& value, std::size_t index) {
            return sea ? value.water[index] != 0 : value.buildable[index] != 0;
        });
    if (skeleton.road_path_count != 0) {
        population.traveler_points = pick_points(
            skeleton.terrain, config.road_traveler_points, site_seed ^ kTravelerSalt,
            [](const SiteSkeleton& value, std::size_t index) { return value.roads[index] != 0; });
    }
    WildernessSite result{std::move(skeleton), std::move(population)};
    if (!result.valid_layout()) {
        throw std::logic_error{"荒野 W5 產生無效版面"};
    }
    return result;
}

WildernessSite generate_wilderness_site(const world::RegionTiles& tiles,
                                        world::RegionXY coordinate,
                                        std::uint64_t world_seed, std::uint32_t region_id,
                                        const rules::Ruleset& ruleset) {
    const auto vars = split_site_vars(tiles, coordinate);
    const auto slow = project_wilderness_slow_vars(tiles, coordinate, world_seed, region_id, ruleset);
    const auto seed = derive_site_seed(world_seed, region_id,
                                       static_cast<std::uint16_t>(coordinate.x),
                                       static_cast<std::uint16_t>(coordinate.y));
    return populate_wilderness(build_wilderness_skeleton(slow, seed, ruleset), vars.fast, seed,
                               ruleset);
}

}  // namespace aetheria::site
