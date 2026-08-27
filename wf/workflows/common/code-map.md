# code-map — 原始碼導航

← [common/README](README.md)｜[conventions](conventions.md)｜[INDEX](../../INDEX.md)

**目錄＝職責，檔名＝子題。** 改結構後更新 [refactor](../refactor.md)。

## 檔名慣例

| 型樣 | 意思 |
|---|---|
| `xxx.h` + 同名 `xxx.cpp` | 對外介面與其實作 |
| **門面 header**（`region_generator.h`、`ruleset.h`） | 只有 `#include`；拆檔後保留舊入口 |
| `*_detail.h`、`gen_*.h`、`toml_read.h` | 同目錄 `.cpp` 的 `detail` helper，非公開介面 |
| `stage_*.cpp` | worldgen 階段 1～7 的地形／氣候實作；人文三階段依職責命名 |
| `*_test_support.h` | 該測試目錄專用的 fixture／helper（header-only）|

單一 `.cpp` 的 helper 留匿名 namespace，不進 `detail`。

## 頂層

| 路徑 | 職責 |
|---|---|
| `CMakeLists.txt`、`vcpkg.json` | 專案組態與依賴；來源清單在 `cmake/targets_*.cmake` |
| `cmake/` | target 清單、Godot 工具鏈與 CTest 檢查 |
| `core/` | 純 C++ 玩法核心，**不得依賴 godot-cpp** |
| `core/history/` | payload 無關的 append-only `history_log.*`、雜湊鏈驗證與重放介面 |
| `core/runtime/` | 跨 zone API；`playable_session.*` 編排三層駐留，`turn_commit.*` 解讀玩家命令並協調 journal-first 結算／恢復，`session_persistence.*` 編排世界存讀，`character_save.*` 是獨立角色 codec／列舉入口，`save_raws.*` 管存檔基底 TOML 的不可變複製／雜湊／路徑 |
| `core/site/`、`core/local/`、`core/spatial/` | L1→L2、L2→L3 與共用邊界／切分／歸約 |
| `bridge/` | `AetheriaCore` GDExtension；批次快照／M8 命令與世界槽／角色檔存讀列舉；唯一依賴 godot-cpp |
| `godot/` | `main.gd` UI；只顯示快照並轉發輸入 |
| `tests/` | GoogleTest 單元測試 |
| `sim/` | 不需 Godot 的 headless CLI 探針 |
| `data/` | TOML def 與資料驅動規則 |
| `third_party/godot-cpp/` | 固定 commit 的 submodule checkout |

## `core/` 各領域

### `core/base`、`core/api`、`core/time`

`base/check.h` 不變式檢查；`api/version.*` core 版本 API；`time/tick.*` Tick／Duration 與 360 天曆換算。

### `core/rules` — 不可變 Ruleset

| 檔 | 職責 |
|---|---|
| `ruleset.h` | 對外門面：彙整型別並定義 `Ruleset`／`RulesetLoader` |
| `def_types.h` | id／flag、地形 def 與 `TerrainGroundMapping` |
| `rule_tables.h` | 地形／移動／文明規則，以及 Site 與 Local 生成、填充和建築 def 型別 |
| `power.*`、`ruleset_load_power.cpp` | 力量位階、S、個體門檻與破階 def |
| `power_sources.*`、`ruleset_load_power_sources.cpp` | 三種來源接點、三層魔法、root 信仰、種族上限與 def |
| `combat.*`、`ruleset_load_combat.cpp` | Region 整數戰役公式、可解釋分解、潰散追擊與配額分配 |
| `attributes.*`、`check.*`、`damage.*` | 四屬性與衍生值、單骰 d100 餘量檢定、資料驅動傷害抗性 |
| `toml_read.h` | 內部共用 TOML 讀取／驗證 helper |
| `ruleset.cpp` | `Ruleset` 存取器 + `RulesetLoader::load` 的編排 |
| `ruleset_load_defs.cpp` | terrain／relief／feature／edge 四份 def |
| `ruleset_load_biomes.cpp` | biome 第一命中規則表、movement 季節分母 |
| `ruleset_load_civilization.cpp`、`ruleset_load_factions.cpp` | 現代城市／道路參數；勢力數、影響力、AI LOD 與七項性格 `FactionDef` |
| `ruleset_load_history*.cpp` | 上古歷史數值、結構與引用載入 |
| `ruleset_load_crossings.cpp`、`ruleset_load_world_graph.cpp` | 渡河複合 edge 查表與完整性驗證；手工世界通道宣告與 canonical 排序 |
| `ruleset_load_site*.cpp`、`site_build_rules.h` | Site 地面、F1～F5 與城建循環規則／def 載入 |
| `world_observation_rules.h`、`ruleset_load_world_observations.cpp` | 治安／任務門檻 |
| `ruleset_load_individual.cpp` | `attributes.toml`／`damage.toml` 載入與 fail-fast 驗證 |
| `dungeon_rules.h`、`ruleset_load_dungeon.cpp` | 地城與機關資料 |

`load_*` 是 `RulesetLoader` private static 成員；history detail 只接收入口參考。

### `core/serialize` — zone 位元流

`zone_codec.h` 入口：現行格式 v23，預設只解現行版，v14/v15 需 fixture mode；
`zone_{encode,decode}.cpp` codec；`zone_region_portals.h` portal；`zone_diplomacy_codec.*`
處理 root 外交／情報／AI 與 def id 重映射；`zone_codec_detail.h` 共用檢查；
`registry_codec.h`、`all_components.h` 是 EnTT snapshot（**新 component 只加尾端**），
`world/army_state.h` 是 Region 部隊的 faction／power／玩家控制權威 component；
`local/local_navigation.h` 的 `LocalDoorState` 是 Local 已開門 edge 集合；
`normalized_state_hash.*` 是跨歷史正規化 hash。

### `core/zone` — 生命週期與存檔

`zone_key.h`、`zone.h`、`lod_level.h`；`zone_store.*` 共用契約與記憶體版；
`file_zone_store.*` 磁碟版；`save_manifest.h` 是 store 共用 manifest schema，
`save_manifest_io.*` 負責磁碟 I/O／zstd／manifest codec；`zone_manager.*` 管生命週期與取得／重展開 callback。

### `core/world` — L1 Region 執行期

[core/world 與 AI 詳圖](code-map-world.md)：Region 執行期、外交狀態、AI 知識邊界與相關測試。

[core/site 詳圖](code-map-site.md)：城區骨架 S1～S4、投影／填充、生命週期、歸約與事件升級。

[core/local 詳圖](code-map-local.md)：路線 A/B、垂直層、資料規則與 Site/Local 共用切分。

[core/narrative 與 core/script 詳圖](code-map-narrative-script.md)：湧現任務、事件 feed、Lua 沙箱、受限 Context 與相關測試。

[core/worldgen 詳圖](code-map-worldgen.md)：Region 十二階段生成、內部共用 header、各階段實作與決定論 hash。

## `tests/`

| 目錄 | 內容 |
|---|---|
| `support/` | 跨目錄共用的 ruleset fixture 與固定暖機、min-of-N 效能量測 helper |
| `runtime/` | 三層進退、上層回寫、親自／代管校準、session 磁碟冷存冷讀／pending 拒存、歷史鏈／崩潰恢復／重放／uid／亂數軌跡、獨立角色檔雙角色／世界身分／門共享，以及世界槽自帶 raws 的隔離／竄改拒絕 |
| `narrative/` | 五種湧現任務、運糧／清剿歸約、命運模板與事件 feed |
| `site/` | Site 投影隔離、展開、持久建築、存檔／世界雜湊、效能 |
| `sim/` | 世界級正規化雜湊的跨歷史、磁碟列舉、負向控制與錯誤路徑測試 |
| `time/`、`serialize/` | 曆法邊界與往返；EnTT registry 壓測 |
| `rules/` | `ruleset_*`（載入、錯誤、codec）／`individual_rules`（個體規則驗收）／`power_sources`（魔法、信仰、血統與不可替代性）|
| `world/` | `region_tiles`／`region_step_cost`／`region_path`／`region_turn` |
| `zone/` | `zone_key`／`zone_lifecycle`／`zone_store_contract`（兩種 store 共用契約）／`file_zone_store`／`file_zone_store_manifest`／`zone_codec`／`zone_manager`／`zone_manager_tick`／`cross_zone` |
| `worldgen/` | 地形函式／氣候函式、決定論／參數隔離、輸出／效能／重測；`history_*`（含身分、災變、回饋、隔離）、城市／首都／道路、`influence_spread`／`governance_release`／勢力量測、`portal_*`、晚期隔離；`*_test_support.h` 只供本目錄 fixture/helper |

## `sim/`

`main.cpp` 只接 CLI11。子命令：`gen_commands.*`（Region）、`terrain_metrics.*`（地形量測）、`local_viewer.*`／
`site_viewer.*`（分層 PNG）、`world_hash.*`（只掃 canonical zone 檔，跳過 `chars/`，並提供 `replay <slot>`）。輸出：`debug_canvas.*`（RGB PNG）、
`stage_dump.*`／`pgm_writer.*`（階段 PGM）。**stdout 有 CTest 比對，不要順手改。**
