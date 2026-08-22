# code-map — 原始碼導航

← [common/README](README.md)｜[conventions](conventions.md)｜[INDEX](../../INDEX.md)

**目錄＝職責，檔名＝子題。** 改結構後更新（[refactor](../refactor.md)）。

## 檔名慣例（先讀這段，省得逐檔猜）

| 型樣 | 意思 |
|---|---|
| `xxx.h` + 同名 `xxx.cpp` | 對外介面與其實作 |
| **門面 header**（`region_generator.h`、`ruleset.h`） | 只有 `#include`；拆檔後保留舊入口 |
| `*_detail.h`、`gen_*.h`、`toml_read.h` | 同目錄 `.cpp` 的 `detail` helper，非公開介面 |
| `stage_*.cpp` | worldgen 階段 1～7 的地形／氣候實作；人文三階段依職責命名 |
| `*_test_support.h` | 該測試目錄專用的 fixture／helper（header-only）|

只被單一 `.cpp` 用到的 helper 一律留在該檔匿名 namespace，不進 `detail`。

## 頂層

| 路徑 | 職責 |
|---|---|
| `CMakeLists.txt`、`vcpkg.json` | 專案組態與依賴固定；**來源清單不在這裡**，在 `cmake/targets_*.cmake` |
| `cmake/` | `targets_*.cmake` 來源／測試；`godot_toolchain.cmake` 工具鏈；`check_*.cmake` CTest 檢查 |
| `core/` | 純 C++ 玩法核心，**不得依賴 godot-cpp** |
| `core/runtime/` | 跨 zone 執行期 API；生成 target 不可見 |
| `core/site/`、`core/local/`、`core/spatial/` | L1→L2 Site、L2→L3 Local，以及兩層共用的邊界、切分與歸約機制 |
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
| `ruleset.h` | 對外門面：彙整型別並定義 `Ruleset`／`RulesetLoader` |
| `def_types.h` | id／flag、地形 def 與 `TerrainGroundMapping` |
| `rule_tables.h` | 地形／移動／文明規則，以及 Site 與 Local 生成、填充和建築 def 型別 |
| `power.*`、`ruleset_load_power.cpp` | 力量位階、S、個體門檻與破階 def |
| `attributes.*`、`check.*`、`damage.*` | 四屬性與衍生值、單骰 d100 餘量檢定、資料驅動傷害抗性 |
| `toml_read.h` | 內部共用 TOML 讀取／驗證 helper |
| `ruleset.cpp` | `Ruleset` 存取器 + `RulesetLoader::load` 的編排 |
| `ruleset_load_defs.cpp` | terrain／relief／feature／edge 四份 def |
| `ruleset_load_biomes.cpp` | biome 第一命中規則表、movement 季節分母 |
| `ruleset_load_civilization.cpp`、`ruleset_load_factions.cpp` | 現代城市／道路參數；勢力數與影響力參數 |
| `ruleset_load_history.cpp`、`ruleset_load_history_{values,references}.cpp` | 上古歷史載入編排；數值／結構限制；道路與廢墟引用（共用宣告在 `ruleset_load_history_detail.h`）|
| `ruleset_load_crossings.cpp`、`ruleset_load_world_graph.cpp` | 渡河複合 edge 查表與完整性驗證；手工世界通道宣告與 canonical 排序 |
| `ruleset_load_site*.cpp`、`site_build_rules.h` | Site 地面、F1～F5 與城建循環規則／def 載入 |
| `ruleset_load_individual.cpp` | `attributes.toml`／`damage.toml` 載入與 fail-fast 驗證 |

`load_*` 是 `RulesetLoader` 的 private static 成員；history detail 只接收入口參考。

### `core/serialize` — zone 位元流

`zone_codec.h` 入口；`zone_{encode,decode}.cpp` codec；`zone_region_portals.h` portal 解碼；
`zone_diplomacy_codec.*` 是 root zone 外交持久區塊與 def 字串 id 重映射；
`zone_codec_detail.h` 共用檢查；`registry_codec.h`、`all_components.h` 是 EnTT snapshot
（**新 component 只准加在尾端**）；`normalized_state_hash.*` 做跨歷史正規化 hash。

### `core/zone` — 生命週期與存檔

`zone_key.h`、`zone.h`、`lod_level.h`；`zone_store.*` 共用契約與記憶體版；
`file_zone_store.*` 磁碟版；`save_manifest_io.*` I/O／zstd／manifest；`zone_manager.*` 管生命週期與完整 zone 取得／重展開 callback。

### `core/world` — L1 Region 執行期

[core/world 與 AI 詳圖](code-map-world.md)：Region 執行期、外交狀態、AI 知識邊界與相關測試。

[core/site 詳圖](code-map-site.md)：城區骨架 S1～S4、投影／填充、生命週期、歸約與事件升級。

[core/local 詳圖](code-map-local.md)：路線 A/B、垂直層、資料規則與 Site/Local 共用切分。

### `core/worldgen` — Region 十二階段生成

門面 `region_generator.h` → `region_config.h`（變數、常數、參數 hash）、各階段 header、`region_skeleton.h`／`region_diagnostics.h`；`field_redistribution.*` 是高度／濕度 identity 接縫。

內部共用：`gen_stage_ids.h`、`gen_grid.h`、`gen_noise.h`、`gen_hash.h`；`biome_classification.h` 隔離 terrain／relief 裁決。

實作：`region_seed.cpp`（種子推導與參數 hash）、`stage_plates/height/erosion/climate/rivers/biomes/features.cpp`（階段 1–7；量化閘口在 `stage_erosion.cpp`，地物約束在 `feature_placement.*`）、`civ_tiles.*`（人文階段共用底圖）、`settlement_scoring.cpp`＋`city_scoring.*`（共用純評分）、`city_selection.*`（canonical 分級選點）、`history_layer.cpp`＋`history_roads.*`（階段 8 選址／災變／古道）、`city_sites.cpp`（階段 9）、`road_path.*`＋`road_loops.*`＋`road_network.cpp`（階段 10 工程路徑／MST／補環路）、`portal_candidates.*`＋`portal_boundary_candidates.cpp`＋`portal_generation.cpp`（階段 11 候選、邊界落點與補路）、`capital_selection.cpp`＋`influence_claim.cpp`＋`governance_release.cpp`＋`influence_spread.*`＋`faction_generation.cpp`（階段 12 首都、全域認領、治理釋回與編排）、`region_build.cpp`＋`region_populate.cpp`（骨架／落地）、`region_stage_hash.cpp`＋`region_result_hash.cpp`（決定論 hash）、`region_debug.cpp`（診斷與灰階圖）。

## `tests/`

| 目錄 | 內容 |
|---|---|
| `support/` | 跨目錄共用的 ruleset fixture 與固定暖機、min-of-N 效能量測 helper |
| `site/` | Site 投影隔離、展開、持久建築、存檔／世界雜湊、效能 |
| `sim/` | 世界級正規化雜湊的跨歷史、磁碟列舉、負向控制與錯誤路徑測試 |
| `time/`、`serialize/` | 曆法邊界與往返；EnTT registry 壓測 |
| `rules/` | `ruleset_*`（載入、錯誤、codec）／`individual_rules`（個體規則驗收）|
| `world/` | `region_tiles`／`region_step_cost`／`region_path`／`region_turn` |
| `zone/` | `zone_key`／`zone_lifecycle`／`zone_store_contract`（兩種 store 共用契約）／`file_zone_store`／`file_zone_store_manifest`／`zone_codec`／`zone_manager`／`zone_manager_tick`／`cross_zone` |
| `worldgen/` | 地形函式／氣候函式、決定論／參數隔離、輸出／效能／重測；`history_*`（含身分、災變、回饋、隔離）、城市／首都／道路、`influence_spread`／`governance_release`／勢力量測、`portal_*`、晚期隔離；`*_test_support.h` 只供本目錄 fixture/helper |

## `sim/`

`main.cpp` 只接 CLI11。子命令：`gen_commands.*`（Region）、`terrain_metrics.*`（地形量測）、`local_viewer.*`／
`site_viewer.*`（分層 PNG）、`world_hash.*`（磁碟狀態）。輸出：`debug_canvas.*`（RGB PNG）、
`stage_dump.*`／`pgm_writer.*`（階段 PGM）。**stdout 有 CTest 比對，不要順手改。**
