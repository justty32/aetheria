#pragma once

// local_underground_detail.h：路線 C 實作檔共用的格操作與單層生成結果。
// 只供 core/local 的地下生成實作使用，不是公開 gameplay API。

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/local/local_underground.h"

namespace aetheria::local::underground_detail {

inline constexpr std::size_t kDirections = 4;

[[nodiscard]] constexpr std::size_t tile_index(LocalXY tile) noexcept {
    return static_cast<std::size_t>(tile.y) * kLocalWidth + tile.x;
}

struct LayerBuild {
    LocalTiles tiles;
    std::vector<std::uint8_t> excavated;
    std::vector<UndergroundRoom> rooms;
    std::vector<UndergroundCorridor> corridors;
    LocalXY exit;
};

[[nodiscard]] LayerBuild build_mine_layer(LocalXY entrance, std::int8_t z, std::uint64_t seed,
                                          rules::GroundId ground, rules::EdgeId wall,
                                          rules::EdgeId none);
[[nodiscard]] LayerBuild build_dungeon_layer(LocalXY entrance, std::int8_t z, std::uint64_t seed,
                                             rules::GroundId ground, rules::EdgeId wall,
                                             rules::EdgeId none);
[[nodiscard]] LayerBuild build_ruin_layer(const LocalSlowVars& slow, LocalXY entrance,
                                          std::int8_t z, std::uint64_t seed,
                                          const rules::Ruleset& ruleset,
                                          std::uint32_t& original_segments,
                                          std::uint32_t& removed_segments);

void set_edge(LocalTiles& tiles, LocalXY tile, spatial::BoundarySide side, rules::EdgeId edge);

}  // namespace aetheria::local::underground_detail
