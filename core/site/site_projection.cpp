#include "core/site/site_projection.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
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

[[nodiscard]] std::uint64_t stable_string_hash(std::string_view value) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    for (const auto character : value) {
        hash_byte(hash, static_cast<std::uint8_t>(character));
    }
    return hash;
}

[[nodiscard]] std::uint64_t surface_seed(const SiteSlowVars& slow,
                                         std::uint64_t site_seed,
                                         const rules::Ruleset& ruleset) noexcept {
    auto value = worldgen::splitmix64(
        site_seed ^ stable_string_hash(ruleset.terrain(slow.base)->id));
    value = worldgen::splitmix64(value ^ stable_string_hash(ruleset.relief(slow.relief)->id));
    value = worldgen::splitmix64(value ^ stable_string_hash(ruleset.feature(slow.feature)->id));
    return worldgen::splitmix64(value ^ slow.elevation);
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
           buildable.size() == kSiteTileCount;
}

bool SiteSkeleton::is_buildable(SiteXY tile) const noexcept {
    if (!valid_layout() || tile.x >= kSiteWidth || tile.y >= kSiteHeight) {
        return false;
    }
    const auto index = static_cast<std::size_t>(tile.y) * kSiteWidth + tile.x;
    return buildable[index] != 0;
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
        SiteFastVars{tiles.owner.at(index), tiles.settlement.at(index), tiles.site.at(index)},
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

    SiteSkeleton result;
    result.ground.resize(kSiteTileCount, mapping->ground);
    result.edges.resize(kSiteTileCount * kDirections, *no_edge);
    result.buildable.resize(kSiteTileCount);

    const auto variation_seed = surface_seed(slow, site_seed, ruleset);
    const auto relief_cost = static_cast<std::uint64_t>(ruleset.relief(slow.relief)->move_cost);
    const auto feature_cost = static_cast<std::uint64_t>(ruleset.feature(slow.feature)->move_cost);
    const auto roughness = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        96U, 4U + relief_cost * 12U + feature_cost * 2U + slow.elevation / 1024U));
    for (std::size_t index = 0; index < result.ground.size(); ++index) {
        const auto sample = worldgen::splitmix64(variation_seed ^ index) & UINT64_C(0xFF);
        if (sample < roughness) {
            result.ground[index] = mapping->rough_ground;
        }
    }

    for (std::size_t coordinate = 0; coordinate < kSiteWidth; ++coordinate) {
        const auto north = coordinate;
        const auto south = (kSiteHeight - 1U) * kSiteWidth + coordinate;
        const auto west = coordinate * kSiteWidth;
        const auto east = west + kSiteWidth - 1U;
        result.edges[north * kDirections + kNorth] = slow.edges[kNorth];
        result.edges[east * kDirections + kEast] = slow.edges[kEast];
        result.edges[south * kDirections + kSouth] = slow.edges[kSouth];
        result.edges[west * kDirections + kWest] = slow.edges[kWest];
    }

    for (std::size_t index = 0; index < kSiteTileCount; ++index) {
        const auto* ground = ruleset.ground(result.ground[index]);
        bool buildable = ground != nullptr && (ground->flags & rules::kGroundWaterFlag) == 0;
        for (std::size_t direction = 0; direction < kDirections && buildable; ++direction) {
            const auto* edge = ruleset.edge(result.edges[index * kDirections + direction]);
            buildable = edge != nullptr && (edge->flags & rules::kEdgeRiverFlag) == 0;
        }
        result.buildable[index] = buildable ? UINT8_C(1) : UINT8_C(0);
    }
    return result;
}

std::uint64_t hash_site_skeleton(const SiteSkeleton& skeleton) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_ids(hash, skeleton.ground);
    hash_ids(hash, skeleton.edges);
    hash_integer(hash, static_cast<std::uint64_t>(skeleton.buildable.size()));
    for (const auto buildable : skeleton.buildable) {
        hash_byte(hash, buildable);
    }
    return hash;
}

}  // namespace aetheria::site
