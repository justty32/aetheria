# Site 原始碼導航

本目錄負責 L1→L2 投影、城區／荒野生成、城建、戰鬥、駐留與 Site→Region 歸約。
全部為純 C++；檔名沿用 `site_<職責>_<階段>`，內部共用宣告放 `*_detail.h`。

## 投影與生成

| 檔案 | 職責 |
|---|---|
| `site_projection.h` | 既有 include 門面；保留 SiteZoning、BuildingState、ProceduralBuildingDamage 的 enum 定義路徑 |
| `site_projection_vars.h` | 慢／快變數、格網尺寸與持久座標 |
| `site_projection_layers.h` | 程序／持久／揮發資料層與投影函式宣告 |
| `site_projection.cpp`、`site_skeleton_*.cpp` | Region 變數拆分、seed、S1～S4 骨架與雜湊 |
| `site_population.cpp`、`site_fill_detail.h` | F1～F5 編排、版面驗證與填充共用介面 |
| `site_zoning.cpp`、`site_buildings.cpp`、`site_building_placement.cpp` | F1 分區、F2 建築選取與放置 |
| `site_fortifications.cpp`、`site_fortifications_rings.cpp` | F3 城防編排／邊寫入；城牆環、城門與護城河 |
| `site_fortifications_detail.h` | 城防共用邊界型別、方向偏移與牆環宣告 |
| `site_landmarks.cpp`、`site_damage.cpp` | F4 地標、F5 損毀 |
| `site_wilderness*.{h,cpp}` | W1～W6 荒野邊界、地形、水路、植被、入口與實體 |

## 城建與戰鬥

| 檔案 | 職責 |
|---|---|
| `site_build_loop.h`、`site_build_loop.cpp` | 城建狀態／介面；每小時批次與旬界結算編排 |
| `site_build_loop_commands.cpp` | 狀態存取、進入 FULL 與開始建造 |
| `site_build_economy.cpp`、`site_build_economy_validation.cpp` | 工地進度／每小時經濟；城建狀態驗證 |
| `site_build_loop_detail.h` | 城建命令與經濟結算共用介面 |
| `site_combat.h`、`site_combat.cpp` | 戰鬥介面；回合主迴圈與傷亡配額 |
| `site_combat_attack.cpp`、`site_combat_cohorts.cpp` | 單次攻擊計算；cohort 部署、距離與移動 |
| `site_combat_detail.h` | 戰鬥跨階段共用常數與宣告 |

## 駐留、補算與歸約

| 檔案 | 職責 |
|---|---|
| `site_materialize.h`、`site_materialize.cpp` | 具現化介面；程序層準備與首次展開 |
| `site_materialize_restore.cpp` | digest 恢復與冷重展開 |
| `site_materialize_residency.cpp` | 記憶體內凍結／解凍 |
| `site_materialize_eviction.cpp` | 逐出、卸載與收回 |
| `site_materialize_detail.h` | 程序層準備與 digest 恢復的跨檔宣告 |
| `site_lifecycle.h`、`site_lifecycle.cpp` | digest 介面；驗證、老化與閉式補算 |
| `site_lifecycle_migration.cpp`、`site_lifecycle_migration_placement.cpp` | 骨架失效後的遷移流程；合法位置搜尋 |
| `site_lifecycle_migration_detail.h` | 遷移位置搜尋的共用型別與宣告 |
| `site_streaming.h`、`site_streaming.cpp` | 取得入口、玩家駐留場與 FULL Site 推進 |
| `site_streaming_transitions.cpp` | 回合尾端 LOD 升降級、逐出與統計 |
| `site_reduction.h`、`site_reduction.cpp` | 歸約介面、持久建築與治安量測 |
| `site_reduction_region.cpp` | 城建歸約、Region 回填與 live Site 身分檢查 |
| `local_reduction_schema.h`、`site_event_escalation.*` | Local→Site 歸約資料；持久事件升級到 Region |
