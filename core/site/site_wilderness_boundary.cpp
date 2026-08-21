#include "core/site/site_wilderness.h"

#include "core/site/site_wilderness_boundary_detail.h"
#include "core/spatial/boundary_profile.h"

#include <cstddef>
#include <stdexcept>

namespace aetheria::site {

WildernessSlowVars project_wilderness_slow_vars(const world::RegionTiles& tiles,
                                                 world::RegionXY coordinate,
                                                 std::uint64_t world_seed,
                                                 std::uint32_t region_id,
                                                 const rules::Ruleset& ruleset) {
    if (!tiles.valid_layout()) {
        throw std::runtime_error{"無法從版面無效的 RegionTiles 投影荒野邊界"};
    }
    WildernessSlowVars result;
    result.local = split_site_vars(tiles, coordinate).slow;
    const wilderness_boundary_detail::RegionBoundarySource source{tiles, ruleset};
    const auto parent_seed = world_seed ^ static_cast<std::uint64_t>(region_id);
    for (std::size_t side = 0; side < result.boundaries.size(); ++side) {
        result.boundaries[side] = spatial::build_boundary_profile(
            source, static_cast<std::uint32_t>(coordinate.x),
            static_cast<std::uint32_t>(coordinate.y), static_cast<SiteBoundarySide>(side),
            parent_seed, ruleset);
    }
    return result;
}

}  // namespace aetheria::site
