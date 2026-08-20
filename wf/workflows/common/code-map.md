# code-map — 原始碼導航

← [common/README](README.md)｜[conventions](conventions.md)｜[INDEX](../../INDEX.md)

**目錄＝職責邊界，檔名＝該職責內的子題。** 找東西先定目錄再看檔名；改完結構要回來更新本檔（維護鏈見 [refactor](../refactor.md)）。

## 檔名慣例（先讀這段，省得逐檔猜）

| 型樣 | 意思 |
|---|---|
| `xxx.h` + 同名 `xxx.cpp` | 對外介面與其實作 |
| **門面 header**（`region_generator.h`、`ruleset.h`） | 內容只有 `#include`。被很多地方 include 的舊入口，拆檔後保留原檔名讓呼叫端不必改 |
| `*_detail.h`、`gen_*.h`、`toml_read.h` | **內部**共用 helper，放 `detail` sub-namespace，只給同目錄的 `.cpp` 用，不是對外介面 |
| `stage_*.cpp` | worldgen 階段 1～7 的地形／氣候實作；人文三階段依職責命名 |
| `*_test_support.h` | 該測試目錄專用的 fixture／helper（header-only）|

只被單一 `.cpp` 用到的 helper 一律留在該檔匿名 namespace，不進 `detail`。

## 頂層

| 路徑 | 職責 |
|---|---|
| `CMakeLists.txt`、`vcpkg.json` | 專案組態與依賴固定；**來源清單不在這裡**，在 `cmake/targets_*.cmake` |
| `cmake/` | 建置分檔：`targets_core/tests/sim/bridge.cmake`（四 target 的來源與測試登記）、`godot_toolchain.cmake`（Godot 偵測／API dump／submodule revision）、`check_*.cmake`（CTest 用的隔離與跨行程腳本）|
| `core/` | 純 C++ 玩法核心，**不得依賴 godot-cpp** |
| `bridge/` | `AetheriaCore` Node 與 GDExtension 註冊；唯一可 include godot-cpp 的自有目錄 |
| `godot/` | 純顯示／呼叫驗證場景與 `.gdextension` 描述檔 |
| `tests/` | GoogleTest 單元測試 |
| `sim/` | 不需 Godot 的 headless CLI 探針 |
| `data/` | TOML def 與資料驅動規則 |
| `third_party/godot-cpp/` | 固定 commit 的 submodule checkout |

## `core/` 各領域

### `core/base`、`core/api`、`core/time`

`base/check.h` 所有建置組態都生效的不變式檢查；`api/version.*` core 對外 API（目前只有版本）；`time/tick.*` Tick／Duration 與 360 天曆換算。

### `core/rules` — 不可變 Ruleset

| 檔 | 職責 |
|---|---|
| `ruleset.h` | 入口：include 下面兩個型別 header + `Ruleset`／`RulesetLoader` 兩個 class |
| `def_types.h` | id 型別、flag 常數、`Terrain/Relief/Feature/EdgeDef` |
| `rule_tables.h` | 獨立 `TerrainRule`／`ReliefRule`、`MovementRules`、`CrossingRule`、世界通道與 `CivilizationRules` |
| `toml_read.h` | 內部共用 TOML 讀取／驗證 helper |
| `ruleset.cpp` | `Ruleset` 存取器 + `RulesetLoader::load` 的編排 |
| `ruleset_load_defs.cpp` | terrain／relief／feature／edge 四份 def |
| `ruleset_load_biomes.cpp` | biome 第一命中規則表、movement 季節分母 |
| `ruleset_load_civilization.cpp`、`ruleset_load_factions.cpp`、`ruleset_load_history.cpp` | 現代城市／道路參數；勢力數與影響力參數；上古歷史參數 |
| `ruleset_load_crossings.cpp`、`ruleset_load_world_graph.cpp` | 渡河複合 edge 查表與完整性驗證；手工世界通道宣告與 canonical 排序 |

各段是 `RulesetLoader` 的 **private static 成員函式**（自由函式碰不到 `Ruleset` 的 private）。

### `core/serialize` — zone 位元流

`zone_codec.h` 入口 / `zone_encode.cpp`（`encode_zone`、`persistent_state_hash`）/ `zone_decode.cpp`（`decode_zone`）/ `zone_region_portals.h`（稀疏 portal 解碼）/ `zone_codec_detail.h`（magic、`validate_zone_meta`）/ `registry_codec.h`、`all_components.h`（EnTT snapshot 順序，**新 component 只准加在尾端**）/ `normalized_state_hash.*`（跨歷史正規化 hash）。

### `core/zone` — 生命週期與存檔

`zone_key.h`、`zone.h`、`lod_level.h`；`zone_store.*` 記憶體 store 與共用契約；`file_zone_store.*` 磁碟 store 的類別方法；`save_manifest_io.*` 檔案 I/O、zstd、manifest 編解碼與生成參數比對；`zone_manager.*` 載入／卸載／tick 借用。

### `core/world` — L1 Region 執行期

| 檔 | 職責 |
|---|---|
| `region_tiles.*` | SoA 格資料、稀疏 portal 清單與雙邊一致 edge 寫入 |
| `region_movement.h` | 移動／尋路／旬回合的共同入口 |
| `region_movement_detail.h` | `in_bounds`／`passable`／`manhattan` |
| `region_step_cost.cpp` | 整數 MP 單步成本與季節下限 |
| `region_path.cpp` | A*（admissible heuristic）|
| `region_turn.cpp` | `RegionTurnPipeline` 下令與旬回合推進 |

### `core/worldgen` — Region 十二階段生成

門面 `region_generator.h` → `region_config.h`（慢／快變數、十二階段 config、參數 hash）、`region_seed.h`、`region_relief_stages.h`（1–4）、`region_climate_stages.h`（5–7）、`region_civ_stages.h`（8–10）、`region_late_stages.h`（11–12）、`region_skeleton.h`、`region_diagnostics.h`。

內部共用：`gen_stage_ids.h`（stage id 與高度上下限）、`gen_grid.h`（尺寸檢查、四鄰格、陸地連通分量）、`gen_noise.h`（SplitMix64 stream、value noise、fbm）、`gen_hash.h`（FNV 與灰階化）。`biome_classification.h` 是 terrain／relief 正交裁決的型別隔離入口。

實作：`region_seed.cpp`（種子推導與參數 hash）、`stage_plates/height/erosion/climate/rivers/biomes/features.cpp`（階段 1–7；量化閘口在 `stage_erosion.cpp`，地物放置約束在 `feature_placement.*`）、`civ_tiles.*`（階段 8～12 共用底圖）、`settlement_scoring.cpp`＋`city_scoring.*`（共用純評分）、`city_selection.*`（canonical 分級選點）、`history_layer.cpp`＋`history_roads.*`（階段 8 選址／災變／古道）、`city_sites.cpp`（階段 9）、`road_path.*`＋`road_loops.*`＋`road_network.cpp`（階段 10 工程路徑／MST／補環路）、`portal_candidates.*`＋`portal_boundary_candidates.cpp`＋`portal_generation.cpp`（階段 11 落點與補路）、`capital_selection.cpp`＋`influence_spread.*`＋`faction_generation.cpp`（階段 12 首都與勢力）、`region_build.cpp`（建骨架）、`region_populate.cpp`（落地）、`region_stage_hash.cpp`＋`region_result_hash.cpp`（決定論 hash）、`region_debug.cpp`（陸地比例、連通性、灰階圖）。

## `tests/`

| 目錄 | 內容 |
|---|---|
| `support/` | 跨目錄共用的 `ruleset_fixture.h` |
| `time/`、`serialize/` | 曆法邊界與往返；EnTT registry 壓測 |
| `rules/` | `ruleset_load`／`ruleset_error`（資料不變式錯誤路徑）／`ruleset_zone_codec`（索引重映射）|
| `world/` | `region_tiles`／`region_step_cost`／`region_path`／`region_turn` |
| `zone/` | `zone_key`／`zone_lifecycle`／`zone_store_contract`（兩種 store 共用契約）／`file_zone_store`／`file_zone_store_manifest`／`zone_codec`／`zone_manager`／`zone_manager_tick` |
| `worldgen/` | `region_stage_functions`／`region_determinism`（階段隔離、同種子位元相同）／`region_output_validation`／`region_perf`（十二階段三秒預算）／`history_layer`／`history_feedback`／`history_identity`（權重分家與道路探針）／`history_isolation`／`city_sites`／`road_network`／`portal_stage`／`portal_collision`（落點互異與判準）／`faction_stage`／`late_stage_isolation` |

## `sim/`

`main.cpp` 只有 CLI11 wiring；`gen_commands.*` 是 `gen-region`／`gen-verify` 子命令；`stage_dump.*` 集中十二階段 PGM 輸出；`pgm_writer.*` 輸出灰階 PGM。**stdout 文字有 CTest 在比對**（`SimPersistence`、`SimWorldgen`），不要順手改。
