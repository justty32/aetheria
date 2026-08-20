#pragma once

// influence_spread.h 提供階段 12 將大城選成首都、再以多源移動成本分配勢力的純函式。
// 本檔只定義獨立演算法與暫時的 C++ 參數，不接入 Region 生成管線或持久狀態。

#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/region_civ_stages.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace aetheria::worldgen {

// InfluenceCapital 是單一勢力的 canonical id 與首都格。
// 呼叫端擁有輸入陣列，spread_influence 只在呼叫期間借用。
// faction 0 保留給無主格，不得作為擴散來源。
struct InfluenceCapital {
    world::FactionId faction{};
    world::RegionXY tile;

    constexpr bool operator==(const InfluenceCapital&) const noexcept = default;
};

// InfluenceSpreadConfig 是尚未接資料管線前的影響力預算與季節參數。
// 呼叫端擁有值，spread_influence 只讀取複本。
// max_cost 使用 region_step_cost 的整數 MP 單位，且不得為負。
struct InfluenceSpreadConfig {
    std::int64_t max_cost{};
    std::uint8_t season{1};
};

// select_capitals 只考慮 City，以 Manhattan 最遠點採樣回傳固定選擇順序。
[[nodiscard]] std::vector<CitySite> select_capitals(std::span<const CitySite> cities,
                                                    std::size_t faction_count);

// spread_influence 以全部首都同時進佇列的多源 Dijkstra 回傳逐格 owner；不修改 tiles。
[[nodiscard]] std::vector<world::FactionId>
spread_influence(const world::RegionTiles& tiles, std::span<const InfluenceCapital> capitals,
                 const rules::Ruleset& ruleset, const InfluenceSpreadConfig& config);

}  // namespace aetheria::worldgen
