#include "core/site/site_projection.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace aetheria::site {

bool SiteProceduralLayer::valid_layout() const noexcept {
    return skeleton.valid_layout() && zoning.size() == kSiteTileCount;
}

SiteProceduralLayer populate(SiteSkeleton skeleton, const SiteFastVars& fast) {
    if (!skeleton.valid_layout()) {
        throw std::runtime_error{"無法填充版面無效的 SiteSkeleton"};
    }
    if (fast.settlement > world::SettlementTier::City) {
        throw std::runtime_error{"SiteFastVars 含無效 SettlementTier"};
    }

    std::uint8_t density{};
    switch (fast.settlement) {
    case world::SettlementTier::None:
        density = 0;
        break;
    case world::SettlementTier::Village:
        density = 48;
        break;
    case world::SettlementTier::Town:
        density = 96;
        break;
    case world::SettlementTier::City:
        density = 160;
        break;
    }

    SiteProceduralLayer result{std::move(skeleton),
                               std::vector<SiteZoning>(kSiteTileCount, SiteZoning::Open)};
    const auto zoning_seed = worldgen::splitmix64(hash_site_skeleton(result.skeleton) ^
                                                  (static_cast<std::uint64_t>(fast.owner) << 32U));
    for (std::size_t index = 0; index < kSiteTileCount; ++index) {
        const auto sample = worldgen::splitmix64(zoning_seed ^ index) & UINT64_C(0xFF);
        if (result.skeleton.buildable[index] != 0 && sample < density) {
            result.zoning[index] = SiteZoning::Settlement;
        }
    }
    return result;
}

bool valid_persistent_layer(const SitePersistentLayer& layer) noexcept {
    return std::ranges::all_of(layer.buildings, [](const PersistentBuilding& building) {
        return building.tile.x < kSiteWidth && building.tile.y < kSiteHeight &&
               building.type <= BuildingType::SettlementHall &&
               building.state <= BuildingState::Ruined;
    });
}

}  // namespace aetheria::site
