#include "core/site/site_projection.h"

#include "core/site/site_skeleton_detail.h"
#include "core/worldgen/region_seed.h"

#include <stdexcept>
#include <type_traits>

namespace aetheria::site {
namespace {

constexpr std::size_t kDirections = 4;
constexpr std::size_t kNorth = 0;
constexpr std::size_t kEast = 1;
constexpr std::size_t kSouth = 2;
constexpr std::size_t kWest = 3;

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
}

template <typename Value> void hash_integer(std::uint64_t& hash, Value value) noexcept {
    const auto bits = static_cast<std::make_unsigned_t<Value>>(value);
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits >> (byte * 8U)));
    }
}

template <typename Id> void hash_ids(std::uint64_t& hash, const std::vector<Id>& ids) noexcept {
    hash_integer(hash, static_cast<std::uint64_t>(ids.size()));
    for (const auto id : ids) {
        hash_integer(hash, rules::value_of(id));
    }
}

void validate_slow_vars(const SiteSlowVars& slow, const rules::Ruleset& ruleset) {
    if (ruleset.terrain(slow.base) == nullptr) {
        throw std::runtime_error{"SiteSlowVars 含不存在的 TerrainId"};
    }
    if (ruleset.relief(slow.relief) == nullptr) {
        throw std::runtime_error{"SiteSlowVars 含不存在的 ReliefId"};
    }
    if (ruleset.feature(slow.feature) == nullptr) {
        throw std::runtime_error{"SiteSlowVars 含不存在的 FeatureId"};
    }
    for (const auto edge : slow.edges) {
        if (ruleset.edge(edge) == nullptr) {
            throw std::runtime_error{"SiteSlowVars 含不存在的 EdgeId"};
        }
    }
}

}  // namespace

bool SiteSkeleton::valid_layout() const noexcept {
    return ground.size() == kSiteTileCount && edges.size() == kSiteTileCount * kDirections &&
           elevation.size() == kSiteTileCount && water.size() == kSiteTileCount &&
           roads.size() == kSiteTileCount && buildable.size() == kSiteTileCount &&
           city_center.x < kSiteWidth && city_center.y < kSiteHeight;
}

bool SiteSkeleton::is_water(SiteXY tile) const noexcept {
    if (!valid_layout() || tile.x >= kSiteWidth || tile.y >= kSiteHeight) {
        return false;
    }
    return water[detail::tile_index(tile)] != 0;
}

bool SiteSkeleton::is_road(SiteXY tile) const noexcept {
    if (!valid_layout() || tile.x >= kSiteWidth || tile.y >= kSiteHeight) {
        return false;
    }
    return roads[detail::tile_index(tile)] != 0;
}

bool SiteSkeleton::is_buildable(SiteXY tile) const noexcept {
    if (!valid_layout() || tile.x >= kSiteWidth || tile.y >= kSiteHeight) {
        return false;
    }
    return buildable[detail::tile_index(tile)] != 0;
}

SiteProjectionVars split_site_vars(const world::RegionTiles& tiles, world::RegionXY coordinate) {
    if (!tiles.valid_layout()) {
        throw std::runtime_error{"無法從版面無效的 RegionTiles 切出 Site 變數"};
    }
    const auto index = tiles.index_of(coordinate);
    const auto edge_offset = index * kDirections;
    return {
        SiteSlowVars{tiles.base.at(index),
                     tiles.relief.at(index),
                     tiles.feature.at(index),
                     tiles.elevation.at(index),
                     {tiles.edges.at(edge_offset + kNorth), tiles.edges.at(edge_offset + kEast),
                      tiles.edges.at(edge_offset + kSouth), tiles.edges.at(edge_offset + kWest)}},
        SiteFastVars{tiles.owner.at(index), tiles.settlement.at(index), tiles.site.at(index),
                     tiles.reduction_value<world::PopulationReduction>(coordinate),
                     tiles.reduction_value<world::DevelopmentLevelReduction>(coordinate),
                     tiles.reduction_value<world::FoodStockReduction>(coordinate),
                     tiles.reduction_value<world::ProductionStockReduction>(coordinate),
                     tiles.defense.at(index), tiles.damage.at(index)},
    };
}

std::uint64_t derive_site_seed(std::uint64_t world_seed, std::uint32_t region_id, std::uint16_t x,
                               std::uint16_t y) noexcept {
    return worldgen::splitmix64(
        world_seed ^ static_cast<std::uint64_t>(region_id) ^
        ((static_cast<std::uint64_t>(y) << 16U) | static_cast<std::uint64_t>(x)));
}

SiteSkeleton build_site_skeleton(const SiteSlowVars& slow, std::uint64_t site_seed,
                                 const rules::Ruleset& ruleset) {
    validate_slow_vars(slow, ruleset);
    const auto* mapping = ruleset.terrain_ground_mapping(slow.base);
    const auto no_edge = ruleset.find_edge("edge.none");
    if (mapping == nullptr) {
        throw std::runtime_error{"TerrainId 缺少 Site ground 映射"};
    }
    if (no_edge == std::nullopt) {
        throw std::runtime_error{"Site 骨架缺少 EdgeDef：edge.none"};
    }
    if (!ruleset.site_generation_rules().loaded) {
        throw std::runtime_error{"Site 骨架缺少 city_skeleton 資料規則"};
    }

    SiteSkeleton result;
    result.ground.resize(kSiteTileCount, mapping->ground);
    result.edges.resize(kSiteTileCount * kDirections, *no_edge);
    result.elevation.resize(kSiteTileCount);
    result.water.resize(kSiteTileCount);
    result.roads.resize(kSiteTileCount);
    result.buildable.resize(kSiteTileCount);
    detail::generate_site_terrain(result, slow, site_seed, ruleset);
    detail::generate_site_roads(result, slow, site_seed, ruleset);
    detail::generate_site_blocks(result, site_seed, ruleset);
    detail::mark_site_buildable(result, ruleset);
    return result;
}

std::uint64_t hash_site_skeleton(const SiteSkeleton& skeleton) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_ids(hash, skeleton.ground);
    hash_ids(hash, skeleton.edges);
    hash_integer(hash, static_cast<std::uint64_t>(skeleton.elevation.size()));
    for (const auto elevation : skeleton.elevation) {
        hash_integer(hash, elevation);
    }
    hash_integer(hash, static_cast<std::uint64_t>(skeleton.water.size()));
    for (const auto water : skeleton.water) {
        hash_byte(hash, water);
    }
    hash_integer(hash, static_cast<std::uint64_t>(skeleton.roads.size()));
    for (const auto road : skeleton.roads) {
        hash_byte(hash, road);
    }
    hash_integer(hash, static_cast<std::uint64_t>(skeleton.buildable.size()));
    for (const auto buildable : skeleton.buildable) {
        hash_byte(hash, buildable);
    }
    hash_integer(hash, skeleton.city_center.x);
    hash_integer(hash, skeleton.city_center.y);
    hash_integer(hash, static_cast<std::uint64_t>(skeleton.gates.size()));
    for (const auto& gate : skeleton.gates) {
        hash_byte(hash, static_cast<std::uint8_t>(gate.side));
        hash_integer(hash, gate.tile.x);
        hash_integer(hash, gate.tile.y);
        hash_integer(hash, rules::value_of(gate.kind));
    }
    hash_integer(hash, static_cast<std::uint64_t>(skeleton.blocks.size()));
    for (const auto& block : skeleton.blocks) {
        hash_integer(hash, block.origin.x);
        hash_integer(hash, block.origin.y);
        hash_integer(hash, block.width);
        hash_integer(hash, block.height);
    }
    return hash;
}

}  // namespace aetheria::site
