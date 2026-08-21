#include "core/worldgen/region_diagnostics.h"

#include "core/worldgen/gen_hash.h"

#include <cstdint>
#include <tuple>
#include <type_traits>

namespace aetheria::worldgen {
namespace {

template <typename Row>
void hash_reduction_row(std::uint64_t& hash, const world::RegionTiles& tiles) noexcept {
    const auto values = tiles.reduction_values<Row>();
    detail::hash_scalar(hash, static_cast<std::uint64_t>(values.size()));
    for (const auto value : values) {
        detail::hash_scalar(hash, value);
    }
}

}  // namespace

std::uint64_t hash_skeleton(const RegionSkeleton& skeleton) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    detail::hash_scalar(hash, skeleton.elevation.width);
    detail::hash_scalar(hash, skeleton.elevation.height);
    detail::hash_vector(hash, skeleton.elevation.meters);
    detail::hash_vector(hash, skeleton.elevation.land);
    detail::hash_scalar(hash, skeleton.elevation.sea_level);
    detail::hash_scalar(hash, hash_stage(skeleton.climate));
    detail::hash_scalar(hash, hash_stage(skeleton.rivers));
    detail::hash_scalar(hash, hash_stage(skeleton.biome));
    detail::hash_scalar(hash, hash_stage(skeleton.features));
    detail::hash_scalar(hash, hash_stage(skeleton.history));
    detail::hash_scalar(hash, hash_stage(skeleton.cities));
    detail::hash_scalar(hash, hash_stage(skeleton.roads));
    detail::hash_scalar(hash, hash_stage(skeleton.portals));
    detail::hash_scalar(hash, hash_stage(skeleton.factions));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.land));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.ocean));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.plain));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.no_feature));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.forest));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.mine));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.oasis));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.landmark));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.ancient_foundation));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.no_edge));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.stream));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.river));
    detail::hash_scalar(hash, rules::value_of(skeleton.definitions.great_river));
    return hash;
}

std::uint64_t hash_tiles(const world::RegionTiles& tiles) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    detail::hash_scalar(hash, tiles.width);
    detail::hash_scalar(hash, tiles.height);
    detail::hash_vector(hash, tiles.base);
    detail::hash_vector(hash, tiles.relief);
    detail::hash_vector(hash, tiles.feature);
    detail::hash_vector(hash, tiles.temperature);
    detail::hash_vector(hash, tiles.moisture);
    detail::hash_vector(hash, tiles.elevation);
    detail::hash_vector(hash, tiles.edges);
    detail::hash_vector(hash, tiles.owner);
    detail::hash_vector(hash, tiles.settlement);
    std::apply(
        [&](auto... row) {
            (hash_reduction_row<std::remove_cvref_t<decltype(row)>>(hash, tiles), ...);
        },
        world::RegionReductionRows{});
    detail::hash_scalar(hash, static_cast<std::uint64_t>(tiles.portals.size()));
    for (const auto& portal : tiles.portals) {
        detail::hash_scalar(hash, portal.tile.x);
        detail::hash_scalar(hash, portal.tile.y);
        detail::hash_scalar(hash, rules::value_of(portal.channel));
    }
    detail::hash_scalar(hash, static_cast<std::uint64_t>(tiles.site.size()));
    for (const auto& site : tiles.site) {
        detail::hash_scalar(hash, site.lod);
        detail::hash_scalar(hash, static_cast<std::uint8_t>(site.has_live_site));
        detail::hash_scalar(hash, static_cast<std::uint8_t>(site.ever_realized));
    }
    return hash;
}

}  // namespace aetheria::worldgen
