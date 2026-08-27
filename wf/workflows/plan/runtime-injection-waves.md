# M10 波次細目（波 0–1）：地基與存檔模型

← [runtime-injection.md](runtime-injection.md)（總覽）｜波 2 → [runtime-injection-inject.md](runtime-injection-inject.md)｜波 3–5 → [runtime-injection-content.md](runtime-injection-content.md)

## 波 0（三線並行）

### M10.0a 原則五守門測試

- **變因**：一條 ctest：掃 `core/` 標頭裡「帶枚舉子的 `enum class`」，比對白名單。
- 白名單兩段：**機制 enum**（`BuildingState`／`LodLevel`／`TurnStage`／`DoorState`／
  `CohortFacing` 等，永久合法）；**待清償**（現有 8 個違規：`CohortRole`、
  `PersistentBuildingType`、`FactionGoal`、`FactionActionKind`、`DungeonArchetype`、
  `TrapKind`、`EmergentQuestKind`、`LocalRoomKind`）——測試斷言**待清償數只准遞減**。
- **Done when**：現況全綠；負向控制＝加一個假內容 enum 會紅；波 3 每清一項就從白名單刪一列。

### M10.0b 雜湊改關係斷言

- **變因**：21 個引用世界雜湊的測試檔、14 個寫死 64-bit 常數（FNV offset basis 之類
  **非基準值，逐條分辨**，`region_determinism_test.cpp` 占 6 個）改成關係斷言：
  存→讀相等、正反序建構相等、冷載入相等、跨執行兩次相等。
- **Done when**：`tests/` 內不再有 golden 世界雜湊常數（留下的每一個都要註解為什麼）；
  負向控制＝故意翻一個 byte 會紅。**先於一切格式變更落地。**

### M10.0c 遊戲會存讀檔（波 0 最大的一輪）

- **變因**：`PlayableSession` 換用 `FileZoneStore`；世界態收進 zone；bridge/godot 開存讀檔入口。
- 範圍：
  1. **用 `ZoneManager` 既有的 root**（建構時已載入或建立 root，`zone_manager.cpp:40-47`
     ——**不要再建第二個**），把 `diplomacy_`（現懸在 `playable_session.h:235`）掛上去
     （`zone.h:123` 明文只有 root 可持有外交真值）；
  2. **`region_`／`battle_site_` 收編進 ZoneManager**（現在是 manager 外的散裝成員
     h:236-237，`save_all()` 救不到它們）——原則九順向：session 變薄；
  3. **部隊的 power／faction 落進 registry component**——`PlayableArmy`（h:63-68）把
     世界態存在 session 成員裡，不落 zone 就存不到；
  4. `store_` 從寫死的 `InMemoryZoneStore` 改成注入 `ZoneStore&`（測試繼續用 InMemory）；
  5. 存＝manager `save_all()` ＋ `write_manifest`（⚠ 順序：manifest 要求 root 已在 store，
     `file_zone_store.cpp:115-117`）；**存檔點語意：只在回合尾端、無 pending 遭遇時可存**
     ——中途態（`encounter_tile_`、進行中戰鬥）不進存檔，也就不必序列化；
  6. **載入路徑與 new_game 路徑分開**：建構子現在無條件重跑 worldgen＋建軍＋外交初始化
     （`playable_session.cpp:99-170`），load 必須改為從 store 重建、再掛回 session 側把手；
  7. bridge 加 `save_game(slot)`／`load_game(slot)`／`list_saves()`；godot 最小 UI（兩顆鈕）。
- **不做**：格式不動（v21）、玩家態不存（那是 M10.2）、不碰 `core/serialize/` 的編解碼本體。
- ⚠ 原則九：存讀檔編排寫成**獨立模組**（如 `core/runtime/session_persistence.*`），
  吃 `ZoneStore&`＋要存的狀態參考；`PlayableSession` 只呼叫它，不長出序列化邏輯。
- **Done when**：玩數旬→存→**關進程重開**→讀，快照與世界雜湊一致；
  `aetheria_sim verify world-hash` 對該槽通過。範本：`tests/zone/diplomacy_save_test.cpp:200-279`。

## 波 1（序列化重地，單線串行）

### M10.1 世界檔自帶 raws

- **變因**：Ruleset 的載入來源從全域 `data/` 改成**存檔內的基底 raws**。
- 範圍：開新世界時把 `data/`（22 個 TOML）整份複製到 `saves/<世界>/raws/`，
  **之後永不改寫**（[裁定#8](runtime-injection-decisions.md)）；讀檔時
  `RulesetLoader::load(save/raws)`；manifest 加 **raws 內容雜湊**——注意它與
  `GenerationParameterHashes` 是**兩個維度並存**（後者只雜湊 12 組 RegionGenerationConfig
  欄位，勢力數等 TOML 輸入不在內，`region_seed.cpp:30-106`），不能互相取代；
  版本 bump → v22（整合輪執行）。
- **呼叫端一起改**（審稿抓的）：`sim` 的 save 導向命令（`verify world-hash` 等）現在
  **先**用 `--data-dir`／編譯期預設載全域 Ruleset 再開存檔（`sim/main.cpp:41,57,92-116`）
  ——必須改成從 `<slot>/raws` 載；bridge 的 `AETHERIA_DEFAULT_DATA_DIR` 是 source tree
  絕對路徑（`cmake/targets_bridge.cmake:9-11`、`targets_sim.cmake:15-17`），
  這輪要定「exported build 的基準 raws 從哪來」（隨執行檔出貨一份 `data/`）。
- ⚠ 測試 fixture：共用的 process-static Ruleset singleton（`tests/support/ruleset_fixture.h:7-9`）
  只准純讀測試用；raws／注入相關的新測試一律**每測試私有 raws 目錄**。
- **不做**：Ruleset 可變性不動（那是 M10.3b）；不做 raws 差分合併，就是整份複製。
- **Done when**：存檔建立後改動全域 `data/`（加地形、改 move_cost、加勢力）→
  舊存檔**照常開且行為不變**（新 DF 行為測試，這順帶解掉「TOML 加勢力弄壞所有舊存檔」
  的 KNOWN-TRAPS——驗收含這個負向控制）；新開世界吃到新 `data/`。

### M10.2 角色檔

- **變因**：玩家態 6 成員（`player_army_id_`、`residence_`、`local_z_`、`local_player_x/y_`、
  `accepted_quest_id_`，`playable_session.h:245-260`）落成獨立檔。
- 範圍：`saves/<世界>/chars/<角色>.bin`，小 codec（cereal PortableBinary），
  **獨立的 `kCharacterFormatVersion`**——不共用世界版本常數，否則世界每 bump 一次
  （v23、v24…）角色檔就無條件失效；一律存字串 id／StableId；bridge 的 save/load 帶
  角色名參數；順手把 `local_door_open_`（h:258）搬進 Local zone——它是世界態。
- ⚠ **world-hash 掃描要教它跳過 `chars/`**：現在它把 manifest 以外每個 `.bin` 都當
  16 碼 ZoneKey 解（`sim/world_hash.cpp:31-41,44-83`），第一個角色檔就會把它弄炸。
- **不做**：`quests_` 不存（衍生快取）；量測／校準成員全不存。
- **Done when**：同一世界兩個角色檔輪流玩互不污染；角色檔往返一致；刪角色檔不影響世界檔；
  world-hash 對含角色檔的存檔照常通過。
