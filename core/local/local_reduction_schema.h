#pragma once

// local_reduction_schema.h：Local 四類可歸約觀測；不含表外玩法欄位。

#include <cstdint>
#include <vector>

#include "core/world/region_tiles.h"

namespace aetheria::local {

struct StructureSegmentState {
    bool damaged{};

    constexpr bool operator==(const StructureSegmentState&) const noexcept = default;
};

struct ControlPointState {
    world::FactionId controller{};

    constexpr bool operator==(const ControlPointState&) const noexcept = default;
};

struct GatheringPointState {
    std::uint32_t remaining{};
    std::uint32_t capacity{};

    constexpr bool operator==(const GatheringPointState&) const noexcept = default;
};

struct PassageState {
    std::uint16_t traversal_cost{};

    constexpr bool operator==(const PassageState&) const noexcept = default;
};

// 每個 vector 對應 lowmap.md 固定量表的一列；空 vector 表示該列沒有觀測。
struct LocalReductionState {
    std::vector<StructureSegmentState> structure_segments;
    std::vector<ControlPointState> control_points;
    std::vector<GatheringPointState> gathering_points;
    std::vector<PassageState> passages;

    bool operator==(const LocalReductionState&) const = default;
};

}  // namespace aetheria::local
