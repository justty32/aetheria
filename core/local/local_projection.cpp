#include "core/local/local_tiles.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace aetheria::local {
namespace {

[[nodiscard]] constexpr std::size_t tile_index(site::SiteXY tile) noexcept {
    return static_cast<std::size_t>(tile.y) * site::kSiteWidth + tile.x;
}

class SiteBoundarySource {
public:
    explicit SiteBoundarySource(const site::SiteProceduralLayer& parent) : parent_{parent} {}

    [[nodiscard]] constexpr std::uint32_t width() const noexcept { return site::kSiteWidth; }
    [[nodiscard]] constexpr std::uint32_t height() const noexcept { return site::kSiteHeight; }
    [[nodiscard]] constexpr std::size_t tile_count() const noexcept {
        return site::kSiteTileCount;
    }
    [[nodiscard]] constexpr std::size_t index(std::uint32_t x, std::uint32_t y) const noexcept {
        return static_cast<std::size_t>(y) * site::kSiteWidth + x;
    }
    [[nodiscard]] std::uint16_t elevation(std::size_t index) const {
        return parent_.skeleton.elevation.at(index);
    }
    [[nodiscard]] rules::GroundId ground(std::size_t index) const {
        return parent_.skeleton.ground.at(index);
    }
    [[nodiscard]] std::uint8_t water_depth(std::size_t index) const {
        return parent_.skeleton.water.at(index);
    }
    [[nodiscard]] rules::EdgeId edge(std::size_t index, spatial::BoundarySide side) const {
        return parent_.edges.at(index * 4U + static_cast<std::size_t>(side));
    }

private:
    const site::SiteProceduralLayer& parent_;
};

[[nodiscard]] std::optional<rules::BuildingDefId> structure_at(
    const site::SiteProceduralLayer& parent, site::SiteXY coordinate) {
    const auto found = std::ranges::find_if(parent.buildings, [&](const auto& building) {
        return coordinate.x >= building.origin.x && coordinate.y >= building.origin.y &&
               coordinate.x < static_cast<std::uint32_t>(building.origin.x) + building.width &&
               coordinate.y < static_cast<std::uint32_t>(building.origin.y) + building.height;
    });
    return found == parent.buildings.end() ? std::nullopt
                                           : std::optional<rules::BuildingDefId>{found->def};
}

}  // namespace

std::uint64_t derive_local_seed(std::uint64_t site_seed, std::uint16_t x,
                                std::uint16_t y) noexcept {
    return worldgen::splitmix64(
        site_seed ^ ((static_cast<std::uint64_t>(y) << 16U) | static_cast<std::uint64_t>(x)));
}

LocalSlowVars project_local_slow_vars(const site::SiteProceduralLayer& parent,
                                      site::SiteXY coordinate, std::uint64_t site_seed,
                                      rules::FeatureId feature,
                                      const rules::Ruleset& ruleset) {
    if (!parent.valid_layout()) {
        throw std::runtime_error{"無法從版面無效的 Site 程序層投影 Local 慢變數"};
    }
    if (coordinate.x >= site::kSiteWidth || coordinate.y >= site::kSiteHeight) {
        throw std::out_of_range{"Local 的父 Site tile 座標超界"};
    }
    if (ruleset.feature(feature) == nullptr) {
        throw std::runtime_error{"Local 慢變數引用不存在的 FeatureId"};
    }
    const auto index = tile_index(coordinate);
    LocalSlowVars result{
        .ground = parent.skeleton.ground[index],
        .overlay = OverlayId::None,
        .edges = {parent.edges[index * 4U], parent.edges[index * 4U + 1U],
                  parent.edges[index * 4U + 2U], parent.edges[index * 4U + 3U]},
        .zoning = parent.zoning[index],
        .structure = structure_at(parent, coordinate),
        .feature = feature,
        .boundaries = {},
    };
    const SiteBoundarySource source{parent};
    for (std::size_t side = 0; side < result.boundaries.size(); ++side) {
        result.boundaries[side] = spatial::build_boundary_profile(
            source, coordinate.x, coordinate.y, static_cast<spatial::BoundarySide>(side),
            site_seed, ruleset);
    }
    return result;
}

}  // namespace aetheria::local
