#include "core/site/site_fill_detail.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace aetheria::site::fill_detail {
namespace {

[[nodiscard]] bool overlaps(const ProceduralBuilding& building,
                            const BuildingPlacement& placement) noexcept {
    const auto building_x1 = static_cast<std::uint32_t>(building.origin.x) + building.width;
    const auto building_y1 = static_cast<std::uint32_t>(building.origin.y) + building.height;
    const auto landmark_x1 = static_cast<std::uint32_t>(placement.origin.x) + placement.width;
    const auto landmark_y1 = static_cast<std::uint32_t>(placement.origin.y) + placement.height;
    return building.origin.x < landmark_x1 && placement.origin.x < building_x1 &&
           building.origin.y < landmark_y1 && placement.origin.y < building_y1;
}

[[nodiscard]] std::uint32_t block_score(const SiteProceduralLayer& layer,
                                        std::size_t index) noexcept {
    const auto& block = layer.skeleton.blocks[index];
    const auto x = static_cast<std::int32_t>(block.origin.x) + block.width / 2;
    const auto y = static_cast<std::int32_t>(block.origin.y) + block.height / 2;
    return static_cast<std::uint32_t>(
        std::abs(x - static_cast<std::int32_t>(layer.skeleton.city_center.x)) +
        std::abs(y - static_cast<std::int32_t>(layer.skeleton.city_center.y)));
}

[[nodiscard]] std::uint16_t side_extent(const SiteBlock& block, SiteBoundarySide side) noexcept {
    return side == SiteBoundarySide::North || side == SiteBoundarySide::South ? block.width
                                                                              : block.height;
}

}  // namespace

void place_site_landmark(SiteProceduralLayer& layer, const SiteFastVars& fast,
                         const rules::Ruleset& ruleset) {
    const auto faction = static_cast<std::uint16_t>(fast.owner);
    const auto& styles = ruleset.site_fill_rules().faction_styles;
    const auto style = std::ranges::find(styles, faction, &rules::FactionLandmarkStyle::faction);
    if (style == styles.end() || style->landmarks.empty()) {
        return;
    }
    const auto sample = worldgen::splitmix64(hash_site_skeleton(layer.skeleton) ^ faction);
    const auto landmark_id = style->landmarks[sample % style->landmarks.size()];
    const auto& landmark = *ruleset.building(landmark_id);

    std::vector<std::size_t> blocks;
    for (std::size_t index = 0; index < layer.block_zoning.size(); ++index) {
        if (layer.block_zoning[index] == zoning_for(landmark.zone)) {
            blocks.push_back(index);
        }
    }
    std::ranges::sort(blocks, [&](std::size_t left, std::size_t right) {
        const auto left_score = block_score(layer, left);
        const auto right_score = block_score(layer, right);
        return left_score != right_score ? left_score < right_score : left < right;
    });

    constexpr std::array sides{SiteBoundarySide::North, SiteBoundarySide::East,
                               SiteBoundarySide::South, SiteBoundarySide::West};
    const std::vector<std::uint8_t> empty_occupancy(kSiteTileCount);
    for (const auto block_index : blocks) {
        const auto& block = layer.skeleton.blocks[block_index];
        for (const auto side : sides) {
            for (std::uint16_t offset = 0; offset < side_extent(block, side); ++offset) {
                const auto placement = placement_for(block, side, offset, landmark);
                if (!valid_building_placement(layer, empty_occupancy, block, placement, side,
                                              zoning_for(landmark.zone))) {
                    continue;
                }
                std::erase_if(layer.buildings, [&](const ProceduralBuilding& building) {
                    return overlaps(building, placement);
                });
                layer.buildings.push_back({landmark_id, placement.origin, placement.width,
                                           placement.height, side,
                                           ProceduralBuildingDamage::Intact});
                return;
            }
        }
    }
}

}  // namespace aetheria::site::fill_detail
