#pragma once

// region_movement_detail.h：region_step_cost.cpp、region_path.cpp、region_turn.cpp
// 共用的內部 helper（原屬 region_movement.cpp 匿名 namespace）。

#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"

#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace aetheria::world::detail {

[[nodiscard]] inline bool in_bounds(const RegionTiles& tiles, RegionXY tile) noexcept {
    return tile.x >= 0 && tile.y >= 0 && static_cast<std::uint32_t>(tile.x) < tiles.width &&
           static_cast<std::uint32_t>(tile.y) < tiles.height;
}

[[nodiscard]] inline bool passable(const RegionTiles& tiles, RegionXY tile,
                                   const rules::Ruleset& ruleset) {
    if (!in_bounds(tiles, tile)) {
        return false;
    }
    const auto* terrain = ruleset.terrain(tiles.base.at(tiles.index_of(tile)));
    if (terrain == nullptr) {
        throw std::runtime_error{"RegionTiles 含不存在的 TerrainId"};
    }
    return (terrain->flags & rules::kTerrainWaterFlag) == 0;
}

[[nodiscard]] inline std::uint32_t manhattan(RegionXY lhs, RegionXY rhs) noexcept {
    return static_cast<std::uint32_t>(std::abs(static_cast<int>(lhs.x) - static_cast<int>(rhs.x)) +
                                      std::abs(static_cast<int>(lhs.y) - static_cast<int>(rhs.y)));
}

}  // namespace aetheria::world::detail
