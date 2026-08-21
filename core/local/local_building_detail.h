#pragma once

// local_building_detail.h：路線 A
// 多個實作檔共用的格索引、邊寫入與內容階段入口。

#include <cstddef>
#include <cstdint>

#include "core/local/local_buildings.h"

namespace aetheria::local::detail {

inline constexpr std::size_t kDirections = 4;

[[nodiscard]] constexpr std::size_t tile_index(std::uint16_t x, std::uint16_t y) noexcept {
    return static_cast<std::size_t>(y) * kLocalWidth + x;
}

[[nodiscard]] LocalTiles make_local_layer(rules::GroundId ground, rules::EdgeId no_edge,
                                          std::uint8_t light);

void set_edge(LocalTiles& tiles, LocalXY tile, spatial::BoundarySide side, rules::EdgeId edge);

void build_house_geometry(BuildingLocalSkeleton& result, const LocalSlowVars& slow,
                          std::uint64_t local_seed, const rules::Ruleset& ruleset);

void fill_furniture(BuildingLocalSkeleton& result, std::uint64_t local_seed,
                    const rules::Ruleset& ruleset);

}  // namespace aetheria::local::detail
