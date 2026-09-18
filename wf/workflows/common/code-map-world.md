# code-map — Region、AI 與 Observer

← [code-map](code-map.md)

## `core/world/` — L1 Region 執行期

本目錄擁有 Region 格資料、部隊、外交與命運真值，協調移動、旬回合、戰鬥及 AI 命令。
格資料採 SoA，含防禦／損毀、私有歸約 row、portal 與 edge 寫入；significance 等級供實體與事件共用。
`region_*.cpp` 依格資料、整數 MP 單步成本、A*、旬推進與模擬分工；
近似公式在第 5 階段結算，計入 live Site 跳過情形。

`named_fate*.cpp` 分 Cohort 權威損失、具名五結果、配額扣抵、抽樣與驗證，
包括離線補算和持久事件 ledger。`combat_scaling*.cpp` 分三層零均值抽樣、升降格與主場計數，
以 Region 為基準，限制 significance 貢獻上界。`*_detail.h` 是各群內部共用 helper。

`diplomacy*.cpp` 分有向關係真值、條約／理由期限、戰爭／厭戰、持久化與知識快照；
提供玩家和談入口，從世界真值產生決定性估計與 FactionView。
`faction_ai*.cpp` 是世界側編排與命令執行，承接 observer 場強 LOD、戰鬥預測、玩家同型命令與同盟參戰。

## `core/ai/` — 受限知識上的決策

此目錄只以快照做決策。`faction_ai*.cpp` 依目標與國力演化分工，涵蓋六目標慣性、
性格效用、faction_id tie-break、三級決策及無偏國力演化；`faction_view.*` 提供快照查詢與公式入口。

`core/ai/include/aetheria/ai/faction_view.h` 是唯一可見的知識快照，不保存 World 參考；
`core/ai/include/aetheria/diplomacy/peace.h` 是玩家與 AI 共用的公開整數和談純函式。
`aetheria_faction_ai_objects` 的 include path **只有 `core/ai/include/`**；
新增 AI 決策實作應加入同一受限 target，不可掛到看得到 repo 根目錄的 target。
隔離由 `cmake/check_faction_view_isolation.cmake` 與 compile-fail 見證驗證。

## `core/observer/` — 共用場強

本目錄提供 strength − travel_cost 純函式，供 Site streaming 與勢力 AI 共用，
不擁有世界真值。外交公式／TOML 載入歸 [Rules](code-map-rules.md)，
外交版本與缺席語意歸 [序列化](code-map-persistence.md)，決策與世界證據見
[測試詳圖](code-map-tests.md)。
