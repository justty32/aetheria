# 執行期注入現況：存檔進行中加入內容，做了沒

← [investigation/README](README.md)｜**已有的地基** [runtime-injection-assets.md](runtime-injection-assets.md)
｜設計裁定 [runtime-injection.md](../../../design/runtime-injection.md)｜[KNOWN-TRAPS](../../KNOWN-TRAPS.md)

**日期**：2026-08-27　**性質**：只調查現況與差距，**不提解決方案**（方案交給下一棒）。

## 問題

使用者原話：「**在遊戲過程中新增地形定義、文化定義、勢力、人物、物品、物品定義**……
**我要的不是熱重載，而是直接在存檔中添加**；預期類似矮人要塞，**世界是一個存檔，人物是一個存檔**。」

拆成可判定的提問：① 六類內容各自能不能在進行中的存檔裡新增？
② 存檔有沒有分成世界／角色兩塊？③ 存檔自不自帶它那份定義表（DF raws）？

## 結論

**六項全都沒有實現。而且比 [runtime-injection.md](../../../design/runtime-injection.md) 記的還差一步：
現在的遊戲根本不會存檔。**

- 可玩 session 用的是 `zone::InMemoryZoneStore`（`core/runtime/playable_session.h:233`），
  `playable_session.cpp`、`bridge/aetheria_core.cpp`、`godot/main.gd` 三處**沒有任何存讀檔呼叫**
  （`main.gd` 只有 `image.save_png`）。
- 會落地磁碟的 `FileZoneStore` 只有**測試與 `sim` CLI** 在用（`sim/world_hash.cpp:86`、`sim/main.cpp`）。

**「在存檔中添加」目前少了兩層：沒有存檔（給玩家的），也沒有添加。**

| 對象 | 現況 | 擋在哪 |
|---|---|---|
| 地形定義 | 改 TOML **重開遊戲**才行；改了舊存檔仍可讀 | `Ruleset` 載入後不可變、無 add API |
| 文化／文明定義 | **型別不存在** | 沒有 `CultureDef`／`CivilizationDef` |
| 勢力 | 只能開局前改 TOML，**加了舊存檔讀不開** | 勢力數建構時定死 + codec 硬比對 |
| 人物 | 容器能長，但全庫唯一的新增是寫死的示範資料 | 沒有人物實體型別、沒有生成入口 |
| 物品 / 物品定義 | **完全不存在** | 沒有 `ItemDef`／`ItemId`／inventory |
| 世界／角色兩塊存檔 | **沒有**，玩家狀態一個位元都沒存 | 玩家狀態全在記憶體成員 |

## 差距

- **沒有存檔功能（前置缺口）**：要談「在存檔中添加」，得先讓遊戲能存能讀。
- **`Ruleset` 不可變、且明文不進存檔**：拷貝/賦值 `= delete`、建構子 private、只有
  `RulesetLoader` 是 friend（`core/rules/ruleset.h:33-41,152-154`），公開介面全 const，
  **沒有任何 add/insert/register**。[definitions.md](../../../design/definitions.md) 裁定
  「def 不進 registry、不進存檔」——**與矮人要塞模型直接衝突**，
  要動的是設計裁定，不只是補程式碼。
- **勢力數是硬邊界，而且會反咬舊存檔**：`[factions].faction_count` 從 TOML 讀入，
  且要求 `faction_defs` 逐一對應（`core/rules/ruleset_load_factions.cpp:32-42,104-106`）；
  `WorldDiplomacyState(faction_count, ...)` 建構時把 `(n+1)²` 關係矩陣一次配好
  （`core/world/diplomacy.h:101`、`core/world/diplomacy.cpp:39-53`），無 `add_faction`；
  `PlayableSession` 更是把 `3` 寫死（`core/runtime/playable_session.cpp:103`）。
  最硬的一條：`core/serialize/zone_diplomacy_codec.cpp:57,151` 檢查
  「存檔勢力數 ≠ 目前 Ruleset ⇒ throw」。
  ⇒ **今天在 TOML 加第四個勢力，所有既有存檔立刻讀不開。**這正是 DF raws 的反面。
- **人物不是實體**：`PersistentNamedNpc` = uid + 兩個 name key + 兩個 bool
  （`core/site/site_projection.h:118-129`），沒有屬性／勢力／位階。
  `NamedFateLedger::members` 是 `std::vector`（容量上能長，`core/world/named_fate.h:119-128`），
  但全庫唯一的 `push_back` 是 `core/runtime/playable_session.cpp:953-962` 的寫死示範資料
  （`uid=9001`、`"守軍隊長艾琳"`），不是可重複呼叫的生成入口。
  `data/attributes.toml` 是**公式參數**，不是人物目錄。
- **文明不是實體**：`data/civilization.toml` 是城市評分權重、聚落數量目標、道路工程參數
  這類 worldgen 常數，`Ruleset` 只持有單一份 `civilization_rules_`（`core/rules/ruleset.h:113,175`）。
- **物品是零**：全庫無 `ItemDef`／`ItemId`／inventory。最接近的 `FurnitureDef` 是
  Local 場景生成密度規則。傳奇物品的來源
  [unique-objects.md](../../../design/unique-objects.md) 是使用者裁定**擱置**的。
- **角色那塊存檔完全不存在**：駐留層、觀察點座標、接下的任務、玩家部隊 id 全是
  `PlayableSession` 的記憶體成員（`playable_session.h:245,250,254-260`），
  不在 `AllComponents`（`core/serialize/all_components.h:22-24`）也不在 manifest。
- **沒有歷史日誌**：全 repo 搜不到 event log／command log／event sourcing。
  裁定要的「重放一致」**沒有任何載體**。`PlayableSession::events_` 是給 UI 消費的
  執行期佇列，不落地。
- **原則五的三個破口正好擋在路上**：`PersistentBuildingType` 六值寫死**而且被序列化進存檔**
  （`core/site/site_projection.h:94`）、`CohortRole` 四值寫死且沒有 `units.toml`
  （`core/site/site_combat.h:17`）、`FactionGoal`／`FactionActionKind` 寫死且
  `Develop`／`Prepare`／`Expand`／`StatisticalProgress` 都是空實作
  （`core/ai/include/aetheria/ai/faction_ai.h:16-33`）。
- **Lua 層幫不上忙**：沙箱 `_ENV` 只剩 16 個 key、`Context` 唯讀
  （`core/script/script_engine.cpp:194,246-328`），腳本無法建立內容。

## 牽扯到的部份

- **存檔版本**：`kSaveFormatVersion = 21`（`core/serialize/zone_codec.h:13`）。政策是
  **版本不符 fail-fast、不寫遷移**（[zone-save-history.md](../../../design/zone-save-history.md)），
  且歷史上撞過兩次版本號 → 這件事至少要**獨佔一次版本號**。
- **測試**：引用世界雜湊的檔案**實數 21 個**；`tests/` 內寫死的 64-bit 常數**實數 14 個**
  （`region_determinism_test.cpp` 占 6 個；其中如 `14695981039346656037` 是 FNV offset basis
  不是基準值，**要逐條分辨**）。[KNOWN-TRAPS](../../KNOWN-TRAPS.md) 已記「該改成關係斷言」。
  ⚠ `runtime-injection.md` 寫的「12 個檔、11 個常數」與這次實測不符，以本檔為準。
- **判準**：M9 的「同存檔跑兩次、每個檢查點雜湊相同」要改寫成
  「**同存檔 + 同注入序列**跑兩次相同」。
- **會被碰到的目錄**：`core/rules/`（不可變性）、`core/serialize/`（id 表 + 版本）、
  `core/world/diplomacy*`（勢力數）、`core/zone/`（存檔根、兩塊存檔）、
  `core/runtime/playable_session*`（玩家狀態 + 換掉 InMemoryZoneStore）、
  `bridge/` 與 `godot/`（存讀檔入口是全新的）。
