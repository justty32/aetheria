#pragma once

// zone_region_portals.h 收斂 v8 Region portal 稀疏清單的 archive 讀取。

#include "core/world/region_tiles.h"

#include <cstdint>
#include <stdexcept>

namespace aetheria::serialize::detail {

template <typename Archive>
void load_region_portals(Archive& archive, world::RegionTiles& tiles) {
    std::uint64_t portal_count{};
    archive(portal_count);
    if (portal_count > 4096U) {
        throw std::runtime_error{"zone RegionTiles portal 稀疏清單過大"};
    }
    tiles.portals.reserve(static_cast<std::size_t>(portal_count));
    for (std::uint64_t index = 0; index < portal_count; ++index) {
        world::RegionPortal portal;
        std::uint32_t channel{};
        archive(portal.tile.x, portal.tile.y, channel);
        portal.channel = static_cast<rules::WorldConnectionId>(channel);
        tiles.portals.push_back(portal);
    }
}

}  // namespace aetheria::serialize::detail
