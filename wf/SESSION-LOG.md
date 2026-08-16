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

- （無）

### 規劃者（Opus 5）

- **M0.6 全部通過**（含 M0.6.1 分桶修正）。分桶分布已獨立驗證：
  61,440 個 Site key 攤在 **256/256 個桶**、最大桶 1.17×理想，無結構化偏斜。
- **M1.0／M1.0.1 全部通過**（`SpatialPayload` 變體已落地，不變式在建構與 decode 兩路都擋）。
- **M1.1 通過**（管線骨架、量化點唯一、階段隔離已獨立重跑驗證）。
  Region 三階段實測 **2.611 ms / 3 秒預算**，餘裕三個數量級。
- **M1.2 通過**。七階段 + populate **2.942 ms / 3 秒**；量化點的隔離已變成**結構性**的
  （階段 4～7 簽章全吃 `QuantizedElevation`，浮點在型別上進不了下游）。
- **M1.3 通過**。正規化狀態雜湊已落地（per-zone），A* admissible 有負向控制。
- **M1.4 任務書已寄出、等回報** → `wf/inbox/m1_4-cities-roads.md`：選址 + 道路。
  ⚠ **這是整條生成管線第一個「演算法本身順序相依」的階段**（道路重用折扣），
  刻意為之，靠顯式正規順序維持決定論。核心驗收是「打亂輸入順序輸出不變」+ 負向控制。
- **M1.5（出境點／勢力起始／歷史層，階段 10～12）未寫**，之後 M1 即完成。
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
