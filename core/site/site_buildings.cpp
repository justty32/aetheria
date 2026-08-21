#include "core/site/site_fill_detail.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace aetheria::site::fill_detail {
namespace {

[[nodiscard]] std::vector<rules::BuildingDefId> definitions_for(
    const rules::Ruleset& ruleset, rules::SiteFillZone zone) {
    std::vector<rules::BuildingDefId> result;
    for (std::size_t index = 0; index < ruleset.buildings().size(); ++index) {
        if (!ruleset.buildings()[index].landmark && ruleset.buildings()[index].zone == zone) {
            result.push_back(static_cast<rules::BuildingDefId>(index));
        }
    }
    if (result.empty()) {
        throw std::runtime_error{"Site 填充分區缺少 BuildingDef"};
    }
    return result;
}

[[nodiscard]] rules::SiteFillZone fill_zone(SiteZoning zoning) {
    if (zoning == SiteZoning::Residential) {
        return rules::SiteFillZone::Residential;
    }
    if (zoning == SiteZoning::Commercial) {
        return rules::SiteFillZone::Commercial;
    }
    throw std::logic_error{"空地不應進入 F2 建築填充"};
}

[[nodiscard]] std::uint32_t block_buildable_area(const SiteProceduralLayer& layer,
                                                 const SiteBlock& block) {
    std::uint32_t result{};
    for (std::uint16_t y = block.origin.y; y < block.origin.y + block.height; ++y) {
        for (std::uint16_t x = block.origin.x; x < block.origin.x + block.width; ++x) {
            result += layer.skeleton.buildable[tile_index(x, y)] != 0 ? 1U : 0U;
        }
    }
    return result;
}

[[nodiscard]] constexpr std::uint16_t side_extent(const SiteBlock& block,
                                                  SiteBoundarySide side) noexcept {
    return side == SiteBoundarySide::North || side == SiteBoundarySide::South ? block.width
                                                                              : block.height;
}

}  // namespace

void fill_site_buildings(SiteProceduralLayer& layer, const SiteFastVars& fast,
                         const rules::Ruleset& ruleset) {
    const auto& fill = ruleset.site_fill_rules();
    const auto scaled_density = static_cast<std::uint64_t>(fill.base_density_percent) +
                                static_cast<std::uint64_t>(fast.development_level) *
                                    fill.development_density_per_level;
    const auto density = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(scaled_density, fill.max_density_percent));
    std::vector<std::uint8_t> occupied(kSiteTileCount);
    const auto seed = hash_site_skeleton(layer.skeleton);

    constexpr std::array sides{SiteBoundarySide::North, SiteBoundarySide::East,
                               SiteBoundarySide::South, SiteBoundarySide::West};
    for (std::size_t block_index = 0; block_index < layer.skeleton.blocks.size(); ++block_index) {
        const auto zoning = layer.block_zoning[block_index];
        if (zoning == SiteZoning::Open) {
            continue;
        }
        const auto defs = definitions_for(ruleset, fill_zone(zoning));
        const auto& block = layer.skeleton.blocks[block_index];
        const auto target_area = block_buildable_area(layer, block) * density / 100U;
        std::uint32_t occupied_area{};
        const auto block_seed = worldgen::splitmix64(seed ^ block_index);
        const auto first_side = static_cast<std::size_t>(block_seed % sides.size());

        for (std::size_t side_offset = 0;
             side_offset < sides.size() && occupied_area < target_area; ++side_offset) {
            const auto side = sides[(first_side + side_offset) % sides.size()];
            const auto extent = side_extent(block, side);
            for (std::uint16_t offset = 0; offset < extent && occupied_area < target_area;
                 ++offset) {
                const auto sample = worldgen::splitmix64(
                    block_seed ^ (static_cast<std::uint64_t>(side_offset) << 32U) ^ offset);
                for (std::size_t def_offset = 0; def_offset < defs.size(); ++def_offset) {
                    const auto def_id = defs[(sample + def_offset) % defs.size()];
                    const auto& def = *ruleset.building(def_id);
                    const auto placement = placement_for(block, side, offset, def);
                    if (!valid_building_placement(layer, occupied, block, placement, side,
                                                  zoning)) {
                        continue;
                    }
                    mark_building_occupied(occupied, placement);
                    layer.buildings.push_back({def_id, placement.origin, placement.width,
                                               placement.height, side,
                                               ProceduralBuildingDamage::Intact});
                    occupied_area += static_cast<std::uint32_t>(placement.width) * placement.height;
                    break;
                }
            }
        }
    }
}

}  // namespace aetheria::site::fill_detail
