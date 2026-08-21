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

- **待任務書**。

### 規劃者（Opus 5）

> **完成的不留這裡。** M0～M2.3 的結論都在 git log 與 `design/` 裡；
> 下面只留**還沒完成、或會在未來咬人**的東西。

**🔴 main 目前有一個紅燈（收工時的狀態，明天第一件事）**

- ❌ `RegionGenerationStage.EveryTerrainRuleHitsAcrossReferenceSeeds`
  —— `above_three_percent 4 vs 5`、`largest*2 3918 vs land 3686`。
  **這是真的回歸**：M5.7 改了高度分布 → 生態帶分布跟著垮回 4 類。
  ⚠ **但它同時是好消息**：這條 guard 是 M5.6 才加的，**上線一輪就抓到東西**，
  而不是等幾週後有人畫圖才發現。

**明天的第一輪（M5.10）：把 M5.7 的 warp 幅度推回去，同時重新平衡生態帶計分。**
**兩件必須一起做**，它們動的是同一條分布鏈。理由：

> M5.7 第 4 輪（warp 幅度 16）**圖最好、海岸格 +53.6%**，但因勢力校準失敗一路退到幅度 8。
> **綁住它的正是 M5.8 已經修掉的那條錯斷言**——所以現在的幅度 8 **不是調校最佳點，
> 是被一條壞測試壓下來的**。M5.8 已合併，這個約束沒了。

**M5.9 的排序建議（redblobgames 對照，一次只做一項）**

1. 完成 domain warp 並以圖裁決。**noisy edges 與 warp 同尺度是替代，不要疊**
2. 若海岸自然但內陸仍怪：固定 land mask，用 coast distance 取代陸上低頻高度基底
3. 若河谷濕潤帶仍只有一格：river bonus 改成距淡水衰減（不取代風／雨影）

⚠ **不做**：radial island falloff（會把 Region 推成中心島、壓過板塊意圖）、terraces、加 octave。

⚠ **coast distance 的相容性裁定**：L1 Region 整張生成，**相容**；
但**各 Site／Local 自行 flood-fill 會覆寫邊界，與降維裁決鏈衝突**。
下放時必須由 parent 全域先算再經 `BoundaryProfile` 傳邊界值，不能各面自算。

- 📌 **待校準（要有玩法才能判斷，不是現在調）**：`governance_max_cost` 讓無主陸地只剩 1.55%，
  世界第一回合就被瓜分完畢。權衡曲線已量好在 [worldgen-factions.md](../design/worldgen-factions.md)，
  ⚠ 查表**不要只看國界指標**，會被倖存者偏差騙——判準是接觸格保留率。
- ⚠ **已知設計缺口**：同層近距離事件的快速路徑（兩個參與者分屬相鄰 Local zone 的戰鬥），
  `events.md` 沒寫清楚，刻意留到實作撞到再補。記在 `lowmap-streaming.md` 末段。
- **刻意擱置**：mark 與獨特物件的細節；root zone 的成長軸（使用者裁定：過早優化）。
- **未規劃但已知**：垂直層玩法、美術資源工作流。清單見 [design/README.md](../design/README.md)。

**會在未來咬人**

- ⚠ **M4 的驗收不能用 byte 相等**：EnTT snapshot 只對同一段建構歷史決定性。
  世界級正規化雜湊已在 M2.0 落地（`aetheria_sim verify world-hash`）。
- ⚠ **`load` ≠ `rematerialize`**：`load` 只解碼持久層，程序層是空的。
  拿它當重新展開，往返測試**照樣通過**——寫進 [interface-lifecycle.md](../design/interface-lifecycle.md)。
- ⚠ **假通過的三個陷阱**（沒冷載入／值停在預設／只比頭尾），加上最隱蔽的
  **空的層必然通過**。M2.3 三條都踩過邊。寫進 [interface-verification.md](../design/interface-verification.md)。
- ⚠ **混淆點**：「戰鬥位階」與「聚合提升重要性」共用 significance 等級表但升級規則不同。
  見 `power-tiers.md` 末段。

**操作心得**

- 🔧 **派 codex**：`-c model_reasoning_effort="high"` 覆寫 config 的 `ultra`（60 分 → 12 分，品質沒掉）；
  prompt 要明令**不准 fan-out 自我審查子 agent**、**`--parallel 2`**（不帶數字會開滿核心）。
  並行用 `git worktree`，但要挑**檔案真正互斥**的兩件事。
- 🔧 **限流只能按行程樹歸屬，不能按名稱**——名稱比對會打到別的 agent 的同名實例
  （`cc1plus`／`ld`／`housecarl-mcp` 都是）。從 watchdog 往上走 `/proc/<pid>/status` 的 `PPid:`
  找本 session 的 claude 當根（**不要寫死 PID**），找不到根就整個不啟動。
  **不確定歸屬時寄信問，不要動手。** `baloo` 仍是 suspend（`balooctl6 resume` 可恢復）。
- ⚠ **watchdog 的寄信建議需要防抖，目前是單次取樣**。一次 `7z`（1097%，約 11 核）
  的解壓爆量就觸發了「該寄信給 Skyrim agent」，等我追行程樹時它已經結束、CPU 回到 7%。
  **限流動作本身是對的**（只動我方、他方只計數），錯的是**建議**——
  應改成「連續 N 次取樣都超標才建議寄信」。在那之前，**建議只是建議，我要先查證歸屬再決定**。
  順帶：這種爆量正是「效能斷言取 N≥5 次最小值」那條裁定的活證據——
  單次牆鐘取樣若剛好落在這個窗口，`<10 ms` 的預算會假失敗。
- 📬 **與 Skyrim agent 的協定**：收件匣 `~/repo/moddings/skyrim/inbox/`。
  **CPU 我 35%（6 核）／他 45%；GPU 與桌面我全部讓出**，所以他開 Skyrim 不必先問我。
  監控權在我，超標我寄信、他不爭論；反之亦然。Monitor 每 30 秒掃兩邊收件匣。
- ⚠ **貼著 8 KB 上限**：`design/README.md` 因索引成長而超標，已依 AGENTS.md 慣例拆出
  `design/INDEX.md`（README = 入口導引，INDEX = 完整清單）。原始碼最大的是
  `tests/rules/ruleset_error_test.cpp` 7,921。**要動貼邊的檔就先拆**。

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何工作流的進度

- （無）
