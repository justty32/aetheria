# KNOWN-TRAPS — 會在未來咬人的東西

← [SESSION-LOG.md](SESSION-LOG.md)｜[OPS-VERIFY.md](OPS-VERIFY.md)｜[INDEX](INDEX.md)

> **每一條都是踩過或量到才寫下來的**，不是推測。
> 這裡放的是**知道有問題、但當下沒修**的東西——所以它會咬人。
> 驗收方法的心得在 [OPS-VERIFY.md](OPS-VERIFY.md)；派工在 [OPS-NOTES.md](OPS-NOTES.md)。

## 假通過與驗證

- ⚠ **M4 的驗收不能用 byte 相等**：EnTT snapshot 只對同一段建構歷史決定性。
  世界級正規化雜湊已在 M2.0 落地（`aetheria_sim verify world-hash`）。
- ⚠ **`load` ≠ `rematerialize`**：`load` 只解碼持久層，程序層是空的。
  拿它當重新展開，往返測試**照樣通過**——寫進
  [interface-lifecycle.md](../design/interface-lifecycle.md)。
- ⚠ **假通過的三個陷阱**（沒冷載入／值停在預設／只比頭尾），加上最隱蔽的
  **空的層必然通過**。M2.3 三條都踩過邊。寫進
  [interface-verification.md](../design/interface-verification.md)。
- ⚠⚠ **最值得記的一條：三層期望一致只守 `|誤差|<5%` 是不夠的。**
  實測注入 **Site 系統性 +3% → 全綠**，+10% 才紅。
  **3% 的同向偏差正是玩家學得會、而測試看不見的那種。**
  已補「三組比對的正負號不得全部同向」斷言（M7.2／M7.3），實測抓得到。
  ⚠ **日後任何新的期望一致比較，都要同時有「幅度」與「符號」兩條斷言。**
- ⚠ **三個寫死的 Region tile hash 基準**（`8963508752675768512` 等）是會隨 schema
  必然改變的 golden snapshot，不是領域不變量。
  **該改成關係斷言**，別再每次 schema 成長就更新。

## 原則被侵蝕（沒有測試守著的原則會腐蝕）

- 🔴 **原則五「種類是資料，不是 enum」已被侵蝕。**
  18 個內容型別做對了（空 enum + TOML def id），但**兵種**（`CohortRole`）、
  **建築**（`PersistentBuildingType`，2026-08-23 M8.3 **規劃者批准的**）、
  **AI 目標與動作**都是寫死的列舉子。
  **417 個測試全綠也擋不住**——因為原則五只是文件裡的一句話。
  → **動任何一行之前，先把原則五做成一條 ctest 檢查。**
  完整盤點見 [runtime-injection.md](../design/runtime-injection.md)。
- 🔴 **有設計、沒里程碑的文件等於沒有。**
  `rules-extensibility.md` 寫得很完整，但 M0～M9 沒有任何一個判準涉及它，
  所以十輪做下來它一次都沒被實作。
  **沒有里程碑判準掛著的能力，十輪之後也不會存在。**

## 功能缺口（知道是空的）

- 🔴 **遊戲不會存檔**（2026-08-27 量到）：可玩 session 用 `zone::InMemoryZoneStore`
  （`core/runtime/playable_session.h:233`），`bridge/` 與 `godot/main.gd` 沒有任何存讀檔呼叫；
  會落地的 `FileZoneStore` 只有測試與 `sim` CLI 在用。
  **M8.1「遊戲第一次能玩」不含存讀檔**，而所有存檔相關的判準都只在測試裡跑過。
- 🔴 **在 TOML 加一個勢力，所有既有存檔立刻讀不開**：
  `core/serialize/zone_diplomacy_codec.cpp:57,151` 檢查「存檔勢力數 ≠ 目前 Ruleset ⇒ throw」。
  這是「存檔自帶 raws」的**反面**，直接擋住執行期注入。
- ⚠ **def 的 remap 只驗字串 id 在不在，不驗內容**：把 `terrain.swamp` 的 `move_cost` 改掉，
  舊存檔照樣載入、沒有任何警告（`core/serialize/zone_decode.cpp:26-44`）。
  與「生成參數雜湊要進 manifest」是同一類問題，但 def 這邊沒做。
- ⚠ **原則七的注入插入點是死代碼**：`ZoneManager::queue_materialize/unload/destroy`
  + `flush_commands()`（`core/zone/zone_manager.h:106-108`）語意正確但
  `core/`、`bridge/` 內**沒有任何呼叫端**；`TurnStage::TurnEnd`
  （`core/world/region_turn.cpp:181`）只是一行 `notify`，是空的。
- 🔴 **named fate 的持久化路徑是斷的**（2026-08-27 量到）：`resolve_encounter` 裡的
  `NamedFateLedger` 是**函式內區域變數**（`core/runtime/playable_session.cpp:953`），
  用完即丟、從未 emplace 進任何 zone registry——它在 `AllComponents` 白名單裡、
  存檔測試也全綠（`tests/world/named_fate_test.cpp:268` 是自己 emplace 的），
  但**可玩流程產生的具名命運一個位元都不會落地**。M10.6a 排定接上。
- 🔴 **AI 在地圖上什麼都不做**：`Develop`／`Prepare`／`Expand`／`StatisticalProgress`
  四個動作在 `execute_faction_command` 裡都是 `break;`，只改抽象國力數字。
  **不會派兵、不會建城。**
- ⚠ **Region 公式沒有建築組成輸入**：五棟城建的 Site Development 是 9，Region 仍是 1，
  1000/1000 樣本不一致（M8.3 量到）。**現在「蓋東西」在大地圖尺度上幾乎沒有意義。**
- ⚠ **沒有「建造領主廳」命令**：住宅走完整施工 pipeline，領主廳只能用既有持久物件
  fixture 驗證。不能說兩者都有玩家施工流程。

## 混淆點

- ⚠ 「戰鬥位階」與「聚合提升重要性」共用 significance 等級表但升級規則不同。
  見 `power-tiers.md` 末段。
- ⚠ **熱重載 ≠ 執行期注入**：前者保證不動世界狀態，後者要動。
  混為一談了九個里程碑，見 [runtime-injection.md](../design/runtime-injection.md)。

## 已解決但值得留著的

- ✅ **`p` 已有測試守著**（M6.7 補的）：`R=1.2` 時 p=1.0／1.3／2.0 的 B 方損失
  **16660／17470／19540**，斷言嚴格遞增。
- ⚠ **並行派工要把「誰有權動存檔版本號」劃給單一路**——沒劃，一晚撞了兩次
  （兩個 v16、兩個 v17），每次都要另開一輪整合。
