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

// InfluenceTerrainStepInput 是影響力進入目的格時唯一可見的成本資料。
// edge 與起點刻意不在此型別中，階段 12 不能讀取階段 10 的路網。
struct InfluenceTerrainStepInput {
    rules::TerrainId terrain;
    rules::ReliefId relief;
    rules::FeatureId feature;
    std::uint8_t season{};
};

// InfluenceClaimMap 保存全域認領後每格的 canonical owner 與到其首都的成本。
// 呼叫端擁有兩個等長陣列；水格維持 owner 0 與無限成本。
struct InfluenceClaimMap {
    std::vector<world::FactionId> owner;
    std::vector<std::int64_t> capital_cost;
};

// InfluenceSpreadDiagnostics 記錄全域認領的佇列與同成本重標記量測。
// 呼叫端擁有值；claim_all_land 回傳前完整覆寫。
struct InfluenceSpreadDiagnostics {
    std::uint64_t queue_pushes{};
    std::uint64_t stale_pops{};
    std::uint64_t tie_relabels{};
    std::uint32_t maximum_updates_per_tile{};
};

// select_capitals 只考慮 City，以 Manhattan 最遠點採樣回傳固定選擇順序。
[[nodiscard]] std::vector<CitySite> select_capitals(std::span<const CitySite> cities,
                                                    std::size_t faction_count);

// influence_terrain_step_cost 只計 terrain + relief + feature 與季節倍率。
[[nodiscard]] std::int32_t
influence_terrain_step_cost(const rules::Ruleset& ruleset, InfluenceTerrainStepInput input);

// claim_all_land 以無預算上限的多源 Dijkstra 認領每個可達陸格；不修改 tiles。
[[nodiscard]] InfluenceClaimMap
claim_all_land(const world::RegionTiles& tiles, std::span<const InfluenceCapital> capitals,
               const rules::Ruleset& ruleset, std::uint8_t influence_season,
               InfluenceSpreadDiagnostics* diagnostics = nullptr);

// release_beyond_governance 逐格釋回超過治理成本的認領；結果不依賴走訪順序。
[[nodiscard]] std::vector<world::FactionId>
release_beyond_governance(const InfluenceClaimMap& claims, std::int64_t governance_max_cost);

// spread_influence 依序編排全域認領與治理距離釋回，回傳最終逐格 owner。
[[nodiscard]] std::vector<world::FactionId>
spread_influence(const world::RegionTiles& tiles, std::span<const InfluenceCapital> capitals,
                 const rules::Ruleset& ruleset,
                 const rules::CivilizationRules::FactionRules& factions,
                 InfluenceSpreadDiagnostics* diagnostics = nullptr);

}  // namespace aetheria::worldgen
