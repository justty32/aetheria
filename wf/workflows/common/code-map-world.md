# core/world 與 AI code map

← [總 code map](code-map.md)｜[conventions](conventions.md)

## `core/world` — L1 Region 執行期

| 檔 | 職責 |
|---|---|
| `significance.h` | 實體與事件共用的重要性等級 |
| `region_tiles.*`、`reduction_schema.h` | SoA 格資料（含防禦／損毀）、私有歸約 row、portal 與 edge 寫入 |
| `region_movement.h`、`region_movement_detail.h` | 移動／尋路／旬回合入口與共用 helper |
| `region_step_cost.cpp`、`region_path.cpp` | 整數 MP 單步成本與 A* |
| `region_turn.cpp`、`region_simulation.*` | 旬推進、第 5 階段近似公式與 live Site 跳過計數 |
| `named_fate.*` | Cohort 權威損失、具名五結果、配額扣抵、離線補算與持久事件 ledger |
| `combat_scaling.*` | Region 基準的三層零均值抽樣、升降格、significance 貢獻上界與單一主場計數 |
| `diplomacy.{h,cpp}` | 有向關係矩陣、條約／理由期限、持續戰爭事件與玩家和談入口 |
| `diplomacy_view.cpp` | 世界真值的決定性估計與 FactionView 快照產生 |
| `faction_ai.*` | observer 場強 AI LOD、M6.3 戰鬥預測轉接、玩家同型命令與同盟參戰 |

## 外交的跨目錄邊界

| 路徑 | 職責 |
|---|---|
| `core/rules/diplomacy_rules.h` | 四分量回歸、條約／理由、厭戰與和談的資料型別 |
| `core/rules/ruleset_load_diplomacy.cpp` | `data/diplomacy.toml` 載入與不變式驗證 |
| `core/ai/include/aetheria/ai/faction_view.h` | AI 唯一可見的知識快照；不保存 World 參考 |
| `core/ai/include/aetheria/diplomacy/peace.h` | 玩家與 AI 共用的公開整數和談純函式 |
| `core/ai/faction_view.cpp` | 受限 OBJECT target 的快照查詢與 AI 公式入口 |
| `core/ai/faction_ai.cpp`、`include/aetheria/ai/faction_ai.h` | 六目標慣性、性格效用、faction_id tie-break、三級決策與無偏國力演化 |
| `core/observer/field.h` | Site streaming 與勢力 AI 共用的 `strength - travel_cost` 場強純函式 |
| `tests/rules/diplomacy_rules_test.cpp` | 資料載入、加一筆條約與錯誤速率負向測試 |
| `tests/world/diplomacy_test.cpp` | 有向矩陣、期限、戰爭／厭戰、和談與決定性測試 |
| `tests/world/faction_ai_test.cpp` | 目標切換、性格、LOD 成本／無偏、誤判、均勢、代管、連鎖參戰與效能 |
| `tests/compile_fail/faction_ai_world_truth.cpp` | AI 直接 include 世界真值的負向編譯測試 |

`aetheria_faction_ai_objects` 的 include path 只有 `core/ai/include`；未來 AI 決策實作應加入同一
target，不能改掛到可見 repo 根目錄的 target。CTest 腳本是
`cmake/check_faction_view_isolation.cmake`。

zone v17 由 `zone_diplomacy_codec.*` 在 v15 尾端追加 `FactionTruth`、
`KnowledgeRecord` 與 `FactionMindState`；v15 載入時三者保持明確缺席。
