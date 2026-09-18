# code-map — Bridge、Godot 與 Sim

← [code-map](code-map.md)

## `bridge/` — GDExtension 邊界

此目錄把純 C++ session 接到 Godot，是自有程式碼中依賴 godot-cpp 的綁定層。
`bridge/aetheria_core.h` 是 AetheriaCore 入口；`aetheria_core*.cpp` 按綁定、生成、
session、命令、快照與資料封裝分工，`aetheria_core_<功能群>.cpp` 的後綴指出責任。
`aetheria_core_detail.h` 共用轉接層 helper，不承接玩法真值。

批次快照、可玩命令、世界槽及獨立角色檔的存讀列舉由此轉接；型別註冊仍在同目錄。
新增玩法需落到 core，不能因資料要顯示就把規則寫入封裝函式。

## `godot/` — 呈現與輸入

場景與 GDScript 負責顯示、美術、音效、UI 及輸入轉發。畫面取自核心快照，
可由核心重建，不自行保有回合、單位或地圖玩法狀態。

`main.gd` 是 `main.tscn` 根節點：持節點參考、`_ready`、命令列、輸入轉發與 `_core` 呼叫。
其餘以 `preload` 的 RefCounted **無狀態 static helper** 分職責：`main_ui`／`main_controls`
建畫面（節點參考由 main 的 setter 指派）、`main_format` 摘要／戰報／事件／可見文字、
`main_grid` Site／Local 格線、`main_storage` 新遊戲／存讀檔轉發與內附 data 實體化、
`main_probes` demo／截圖／negative／performance 探針。`region_debug_renderer.gd` 保留
`render()` 編排，疊圖分 `region_debug_layers_terrain`（領界／地物／邊）與
`region_debug_layers_entities`（目標／單位／聚落／portal），色表在 `region_debug_palette`。
helper 一律不持 member／static var，不用 `class_name`。

## `sim/` — Headless CLI 與驗證輸出

`sim/main.cpp` 是 CLI11 命令分派入口。`core_probe*.cpp` 是核心 smoke 探針，
`gen_commands.*` 與 `terrain_metrics.*` 負責 Region 生成及地形量測；
`local_viewer*.cpp`、`site_viewer*.cpp` 各以 `*_viewer_<職責>.cpp` 分出繪圖及輸出工作，
`*_detail.h` 是同組共用介面。RGB PNG、階段 PGM 的共用輸出工具也在此目錄。

`sim/world_hash.cpp` 是世界級正規化 hash 的掃描入口：只掃 canonical zone 檔，
跳過角色檔子目錄；`world_hash_*.cpp` 的 replay 群承接 `replay <slot>` 歷史重放。
掃描與重放使用相同持久語意，錯誤路徑及負向控制由 sim 測試目錄驗證。

**sim stdout 有 CTest 比對，不要順手改。** 驗證與負向控制入口見
[測試詳圖](code-map-tests.md)。
