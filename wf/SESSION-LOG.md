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

- **M2.0 世界級正規化狀態雜湊實作中** → [任務書](inbox/m2_0-world-state-hash.md)。

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
- **M1.5 通過**（95/95 我自己重跑）。三項裁定都落地：存檔升 6→7、古道到河邊就斷（橋塌了）、
  古道自成一組重用折扣。`ruleset_load_crossings` 的 result 唯一性缺口也補上了。
- **M1.5.1（上古身分）已落地** `616fa0a`：上古自己的權重、`ancient_site_count` 24→12、
  間距 `[3,5,8]`→`[5,8,12]`。**但我撤回了它的前提**——我原本說「上古與現代高度重疊是缺陷」，
  那是錯的：羅馬／雅典／大馬士革都在同一點住了幾千年，**現代城市蓋在上古遺址上就是世界本來的樣子**。
  已寫進 [design/worldgen-history.md](../design/worldgen-history.md)。
- ⏸ **M1.5.1（災變脫鉤版）已寫好任務書但刻意未派工** → `wf/inbox/m1_5_1-cataclysm-and-traces.md`。
  中止理由與交接見 `wf/inbox/m1-5-1-halted.md`。仍然認為「災變按分數排序 = 品管不是災變」是對的，
  但優先序低於 M1.6 與檢視器。
- ✅ **M1 完成**（`aeb8301`，109/109 我自己重跑）。十二階段生成管線全部到位、存檔 v8。
  M1.6 三項裁定都落地：`major_city_count = faction_count × 2`、出境點落點規則、portal 稀疏清單。
  M1.6.1 修掉「山口與地下通道保證落在同一格」的結構性缺陷（候選集合是超集 + 同一取最小成本判準）。
- ✅ **M1 後續全部收線**（M1.7～M1.16，125/125）。四條結構性裁定都已寫進設計文件：
  地貌與群系分開裁決、地表濕度是空氣含水量、影響力不得吃道路成本、先全域認領再釋回。
  **國界懸案已結**（+10.23%）、**邊界沒有山地的問題也一併解決**（根因就是 relief 被 moisture 抹平）。
  Region 檢視器已 merge。**這些都不再是 open 項，細節看 git log 與 `design/worldgen-*.md`。**
- 📌 **仍待校準（要有玩法才能判斷，不是現在調）**：`governance_max_cost` 讓無主陸地只剩 1.55%，
  世界第一回合就被瓜分完畢。權衡曲線已量好放在 [worldgen-factions.md](../design/worldgen-factions.md)，
  ⚠ 查表時**不要只看國界指標**，會被倖存者偏差騙——正確判準是接觸格保留率。
- 🔧 **派 codex 的操作心得**：`-c model_reasoning_effort="high"` 覆寫掉 config 的 `ultra`，
  同任務從 ~60 分鐘降到 ~12 分鐘且品質沒掉。另外要在 prompt 明令
  **不准為自我審查 fan-out 子 agent**（他會一口氣開五個審自己的 diff）。
  並行用 `git worktree` 隔離有效，但要挑**檔案真正互斥**的兩件事，否則只是把等待換成合併衝突。
- 🔧 **機器保持安靜的鐵律（同一個錯我犯了兩次才學會）**：
  **限流只能按「行程樹歸屬」，絕不能按「行程名稱」。**
  名稱比對會打到**別的 agent 的同名實例**——`cc1plus`、`ld`、`housecarl-mcp` 都是。
  作法：從 watchdog 自己往上走 `/proc/<pid>/status` 的 `PPid:` 找到本 session 的 claude 當歸屬根
  （**不要寫死 PID，session 重連後會變**），只限流它的子孫；**找不到根就整個不啟動**。
  **不確定歸屬時，寄信問，不要動手。**
  ⚠ 我原本以為 `housecarl-mcp` 是共用 server——**錯的**。它是 stdio MCP，
  **每個 client 各 spawn 一份**（實測同時 9 個實例）。我用名稱比對限流，一次打中全部，
  害它關閉時卡了十分鐘。是 Skyrim agent 查證後寄信糾正我的。
  其他坑：`taskset -cp` 只改主執行緒（多執行緒要 `-a`）；`ps` 的 `%CPU` 是生命週期平均，
  短命行程來不及超過門檻；派工 prompt 要明令 `--parallel 2`（不帶數字會開滿核心）。
  `baloo` 檔案索引仍是 suspend 狀態（`balooctl6 resume` 可恢復）。
- 📬 **與 Skyrim agent 的資源協定（2026-08-21 談定，共八封往返）**：
  對方收件匣 `~/repo/moddings/skyrim/inbox/`（另有 elin／rimworld／tome4 各自的）。
  **CPU 我 35%（上限 6 核）／他 45%；GPU 與螢幕鍵鼠我全部讓出**——
  Godot 一律 headless、檢視器匯出 PNG 讀檔，整晚十六輪沒用過桌面，
  所以**他要開 Skyrim 不必先問我**（省掉往返）。CPU/GPU 監控權在我，超標我寄信給他，
  他不爭論；反之亦然。已架 Monitor 每 30 秒監看兩邊收件匣頂層。
- 📌 **M2 要做世界級正規化雜湊**（M1.3 的是 per-zone）。裁定：它是驗證工具不是執行期狀態，
  直接走訪存檔目錄列舉，不違反成長軸不變量。寫在 `design/zone-save-format.md`。
- 📌 **校準期要回頭看**：雨影探針 leeward moisture 剛好 0。合成探針裡合理，
  但真實地圖若出現連綿不斷的絕對乾燥帶，代表水氣模型太激進。
- ⚠ **M2／M4 的驗收不能用 byte 相等**：EnTT snapshot 只對**同一段建構歷史**決定性，
  不會 canonicalize。跨歷史等價要用正規化狀態雜湊，形狀等 M2 再定。
  寫在 [design/zone-save-format.md](../design/zone-save-format.md)。**這條會在 M4 咬人。**
- ⚠ **貼著 8 KB 上限的檔**（下次要加東西就得先拆，拆法照 [refactor](workflows/refactor.md)，
  拆完更新 [code-map](workflows/common/code-map.md)）：原始碼最大的是
  `tests/rules/ruleset_error_test.cpp` 7,921、`tests/zone/file_zone_store_manifest_test.cpp` 7,858；
  設計文件是 `README.md` 8,189、`interface-lifecycle.md` 8,156、`outline.md` 8,138。
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
