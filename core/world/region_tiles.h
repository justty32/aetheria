#pragma once

#include "core/base/check.h"
#include "core/rules/ruleset.h"
#include "core/zone/lod_level.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace aetheria::world {

// FactionId 是勢力資料表的強型別下標，0 代表無主。
// 世界狀態配發其值，RegionTiles 只保存複本。
// 所屬世界狀態存在期間有效。
enum class FactionId : std::uint16_t {};

// RegionXY 是 L1 Region 內的非環繞方格座標。
// 呼叫端擁有值。
// 值本身永不失效。
struct RegionXY {
    std::int16_t x{};
    std::int16_t y{};

    constexpr auto operator<=>(const RegionXY&) const noexcept = default;
};

// SiteState 是 Region tile 對其下層 Site 的具現化狀態。
// RegionTiles 擁有所有實例。
// 所屬 RegionTiles 析構或重配後失效；lod 不進存檔。
struct SiteState {
    zone::LodLevel lod{zone::LodLevel::Absent};
    bool ever_realized{};

    constexpr bool operator==(const SiteState&) const noexcept = default;
};

// RegionTiles 是 L1 地圖的平行陣列資料與雙邊一致 edge 寫入入口。
// Region Zone 暫時以 optional payload 擁有它，直到三層 layers 形狀另行裁定。
// 所屬 Zone 析構後失效；任何 vector 重配會使先前元素參考失效。
struct RegionTiles {
    RegionTiles() = default;
    RegionTiles(std::uint32_t grid_width, std::uint32_t grid_height);

    [[nodiscard]] std::size_t tile_count() const noexcept;
    [[nodiscard]] std::size_t index_of(RegionXY coordinate) const;
    [[nodiscard]] bool valid_layout() const noexcept;
    void set_edge(RegionXY a, RegionXY b, rules::EdgeId edge_id);
    [[nodiscard]] rules::EdgeId edge_between(RegionXY a, RegionXY b) const;
    [[nodiscard]] std::size_t edge_storage_bytes() const noexcept;
    [[nodiscard]] std::size_t dynamic_storage_bytes() const noexcept;

    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<rules::TerrainId> base;
    std::vector<rules::ReliefId> relief;
    std::vector<rules::FeatureId> feature;
    std::vector<std::uint8_t> temperature;
    std::vector<std::uint8_t> moisture;
    std::vector<std::uint16_t> elevation;
    std::vector<rules::EdgeId> edges;
    std::vector<FactionId> owner;
    std::vector<SiteState> site;
};

}  // namespace aetheria::world
