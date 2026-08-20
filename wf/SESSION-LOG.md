# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](../AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log（若有「已落地功能目錄」則濃縮一句進去）。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾，按工作流／類別**拆檔 + 一個 index 導航**（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

本檔同時 ① 連到各工作流自己的 session-log（若該工作流已長出自己的），② 收**不屬任何工作流**的進度。

> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**（設計決策/修了什麼落到該工作流的文件、待使用者驗的進 [WAIT_USER](WAIT_USER.md)）。完成即整條刪除。

## 最新進度

> **分區塊擁有**：`### 規劃者` 與 `### 實作者` **各自只寫自己那塊，永遠不碰對方那塊**——
> 連順手整理都不行。跨區的事寫信。規約見
> [workflows/inbox/CONTACTS.md](workflows/inbox/CONTACTS.md) 的「同步規約」。
> **in-flight 一定要有記錄**：session 隨時會斷，本檔是唯一交接面。

### 實作者（gpt-sol）

- **M1.5 停工等規劃者裁定**：manifest 9→10 組必改磁碟格式、古道跨河缺複合 edge 表示 →
  `wf/inbox/m1-5-blocked-format-and-ancient-crossings.md`

### 規劃者（Opus 5）

- **M0.6 全部通過**（含 M0.6.1 分桶修正）。分桶分布已獨立驗證：
  61,440 個 Site key 攤在 **256/256 個桶**、最大桶 1.17×理想，無結構化偏斜。
- **M1.0／M1.0.1 全部通過**（`SpatialPayload` 變體已落地，不變式在建構與 decode 兩路都擋）。
- **M1.1 通過**（管線骨架、量化點唯一、階段隔離已獨立重跑驗證）。
  Region 三階段實測 **2.611 ms / 3 秒預算**，餘裕三個數量級。
- **M1.2 通過**。七階段 + populate **2.942 ms / 3 秒**；量化點的隔離已變成**結構性**的
  （階段 4～7 簽章全吃 `QuantizedElevation`，浮點在型別上進不了下游）。
- **M1.3 通過**。正規化狀態雜湊已落地（per-zone），A* admissible 有負向控制。
- **M1.4 通過**（86/86 我自己重跑）。順序相依的正負向控制都到位；瓶頸評分 1.85 ms／12,288 格。
- **裁定：歷史層前置成階段 8**，選址／道路順延成 9／10（2026-08-20）。
  原本排最後會與「上古高分選址 → 現代城市優先落腳」形成**循環依賴**。
  理由寫在 [design/worldgen-civ.md](../design/worldgen-civ.md) 的裁定一節；`worldmap.md` 的管線圖已同步。
- **M1.5 已派工（codex/gpt-sol）、等回報** → 任務書 `wf/inbox/m1_5-history-layer.md`
  + 裁定信 `wf/inbox/m1-5-rulings.md`（**裁定信優先於任務書**）。
  核心驗收是**回饋的負向控制**：`ancient_site_bonus = 0` 時現代城市與上古存活點的重疊必須顯著下降。
- **三條裁定已下**（實作者停工回報兩項阻塞，兩項都成立，是我任務書的疏漏）：
  ① **存檔格式升 6 → 7**——參數 group 9→10 必然改 manifest 位元格式（125→133 bytes）。
  不做遷移：管線重排已讓每個既有世界**語意上失效**，拒絕舊檔是正確行為而非附帶損害。
  ② **古道到河邊就斷**（橋塌了）——不替古道立複合 def，因為複合 def 驗證強制 bridge flag，
  而 bridge flag 語意是「不付渡河代價」，掛給廢渡口是撒謊。
  ③ **古道自成一組重用折扣**——`road_path.cpp` 的重用只看 road flag、**不讀 `move_cost`**，
  所以我原本要求的「古道 move_cost 調高」對道路工程毫無影響，那條作廢。
- **`design/worldgen-civ.md` 已拆檔** → 歷史層獨立成 `design/worldgen-history.md`（裁定 ②③ 在那裡）。
- **M1.6（出境點 + 勢力起始，階段 11～12）未寫**，之後 M1 即完成。
  ⚠ 勢力影響力擴散是**第二個順序相依演算法**（多源洪水填充），tie-break 要顯式定死，
  且要有「打亂勢力輸入順序輸出不變」的正負向控制。M1.6 也是**會動存檔格式**的那一輪（portal 欄位）。
- ⚠ **`ruleset_load_crossings.cpp` 只驗 key 唯一、沒驗 result 唯一**，
  但 `underlying_river()` 是拿 result 反查 river——共用 result 會安靜地把大河降級成小溪。
  現有資料湊巧互異所以沒事。已列進 M1.5 任務書要補。
- 📌 **M2 要做世界級正規化雜湊**（M1.3 的是 per-zone）。裁定：它是驗證工具不是執行期狀態，
  直接走訪存檔目錄列舉，不違反成長軸不變量。寫在 `design/zone-save-format.md`。
- 📌 **校準期要回頭看**：雨影探針 leeward moisture 剛好 0。合成探針裡合理，
  但真實地圖若出現連綿不斷的絕對乾燥帶，代表水氣模型太激進。
- ⚠ **M2／M4 的驗收不能用 byte 相等**：EnTT snapshot 只對**同一段建構歷史**決定性，
  不會 canonicalize。跨歷史等價要用正規化狀態雜湊，形狀等 M2 再定。
  寫在 [design/zone-save-format.md](../design/zone-save-format.md)。**這條會在 M4 咬人。**
- ⚠ **拆檔後仍貼著 8 KB 上限的原始碼**（下次要加東西就得先拆）：
  `tests/zone/file_zone_store_manifest_test.cpp` 7,858、`core/rules/ruleset_load_civilization.cpp` 7,560、
  `core/serialize/zone_decode.cpp` 7,342、`sim/gen_commands.cpp` 7,241、`core/worldgen/city_sites.cpp` 7,155。
  拆法照 [refactor](workflows/refactor.md)，拆完更新 [code-map](workflows/common/code-map.md)。
- ⚠ **兩份設計文件貼著 8 KB 上限**：`interface-lifecycle.md` 8,156、`outline.md` 8,138。
  **下次要動它們就得先拆**。已拆過三次的前例都是同一招：**保留主檔名、切出自足子題**
  （zone-model→+zone-addressing、zone-save→+zone-save-format、medps-relation→+medps-inheritance），
  這樣既有的外部連結大多不必改。
- ⚠ **已知設計缺口**：同層近距離事件的快速路徑（兩個參與者分屬相鄰 Local zone 的戰鬥），
  `events.md` 沒寫清楚，刻意留到實作撞到再補——現在硬定形狀很可能定錯。
  記在 [lowmap-streaming.md](../design/lowmap-streaming.md) 末段。
- **刻意擱置**：mark 與獨特物件的細節；root zone 的成長軸（使用者裁定：過早優化，後面再說）。
- ⚠ **實作時要注意的混淆點**：「戰鬥位階」與「聚合提升重要性」共用 significance 等級表，
  但**升級規則不同**——人多會提升「被個別計算的資格」，不會提升戰鬥位階。見 power-tiers.md 末段。
- **未規劃但已知的缺口**：垂直層玩法、美術資源工作流。
  完整清單見 [design/README.md](../design/README.md) 的「尚未規劃」。

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何工作流的進度

- （無）
