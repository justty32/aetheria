#pragma once

// influence_spread.h 提供階段 12 將大城選成首都、再以多源移動成本分配勢力的純函式。
// 本檔定義階段 12 可獨立驗證的首都選擇與影響力擴散演算法。

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

// InfluenceSpreadDiagnostics 記錄多源擴散的佇列與同成本重標記量測。
// 呼叫端擁有值；spread_influence 回傳前完整覆寫。
struct InfluenceSpreadDiagnostics {
    std::uint64_t queue_pushes{};
    std::uint64_t stale_pops{};
    std::uint64_t tie_relabels{};
    std::uint32_t maximum_updates_per_tile{};
};

// select_capitals 只考慮 City，以 Manhattan 最遠點採樣回傳固定選擇順序。
[[nodiscard]] std::vector<CitySite> select_capitals(std::span<const CitySite> cities,
                                                    std::size_t faction_count);

// spread_influence 以全部首都同時進佇列的多源 Dijkstra 回傳逐格 owner；不修改 tiles。
[[nodiscard]] std::vector<world::FactionId>
spread_influence(const world::RegionTiles& tiles, std::span<const InfluenceCapital> capitals,
                 const rules::Ruleset& ruleset,
                 const rules::CivilizationRules::FactionRules& factions,
                 InfluenceSpreadDiagnostics* diagnostics = nullptr);

}  // namespace aetheria::worldgen
