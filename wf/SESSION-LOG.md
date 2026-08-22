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

## ✅ M5 下層完成（272/272）——但有一條貫穿全程的未解問題

判準「Local 串流與探索」達成：生成（三條路線 + 垂直層）、串流（3×3／5×5、緩衝圈）、
跨 zone 讀與搬移、重新展開、FOV、移動、尋路、Local→Site 歸約，全部落地。

**M5 的真正判準是回頭檢驗 L1↔L2 的抽象**，結果：

| 照出來的 | 是什麼 |
|---|---|
| `OverlayDef` 缺席、慢輸入載體缺欄（feature／礦脈向量）、structure 缺深度欄 | **schema 沒補齊**，加欄位就好 |
| **歸約假設每列必有值** | ⚠ **唯一一張真的反對票**。已裁定改 `optional`，見 [interface-world-mid.md](../design/interface-world-mid.md) |

**沒有需要新的跨層協調、生命週期或事件機制。** 抽象大致站得住。

### ⚠ 未解：產出「能動」但「不像真的」

貫穿 M5 的同一個問題，換了四個地方出現：

| 哪裡 | 症狀 |
|---|---|
| Region 生態帶 | 濕度削頂，swamp ≈ 飽和格（**已交接給地形 session**） |
| 荒野 Local 地表 | 紋理是隨機雜訊，不是成片地貌 |
| 住宅街廓 | 15 棟大小幾乎一樣、排成環形、外圈一圈綠框 |
| **地城** | **9 房排成方格、走廊全是直角、房間大小相同** |

**四個都通過了各自的不變式測試**（可達性 0 不可達、斷頭 0、對稱性 0 mismatch）。
⚠ **這類缺陷測不出來，只有看圖能分辨**——M1 的 97% 沙漠就是這樣被抓到的。

> **裁定：這是 M6 的事，不是 M5 的遺留。**
> M5 的判準是「能不能串流與探索」，那已經達成。
> 「像不像真的」需要**先有玩法內容**才能判斷什麼叫像——
> 現在調美感會重蹈「沒有能看見結果的工具就調數值」那個錯誤的變體：
> **有工具了，但沒有判準。**

## 📦 地形生成：已交接

**獨立一檔** → [TERRAIN-HANDOFF.md](TERRAIN-HANDOFF.md)。
根因（濕度削頂）、量測入口、接縫位置、Freeciv 的 CDF + 配額，都在那裡。

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
- 🔧 **限流的歸屬判斷走「路徑」，不走名稱、也不走行程樹**（前兩種昨天都試過都錯）。
  按名稱會打到別的 agent 的同名實例（`cc1plus`／`ld`／`housecarl-mcp`，
  Skyrim agent 實測同時有 **10 份** houseCARL）。
  ⚠ **按行程樹也錯**：`nohup ... &` 派的 codex 會被 reparent 到 systemd，
  往上找 claude 那條鏈**會斷**——後果是**自己的 build 從來沒被限流**，
  而且它會建議「寄信指控對方」**而那些負載是自己的**。
  正解：`cwd` 落在本 session 的 scratchpad 底下才算自己的。腳本在
  `scratchpad/watchdog-v3.sh`（**寫成檔案不要內嵌 Monitor**，內嵌就改不動）。
- ⚠ **模式比對會打到「自己」——昨晚踩三次**：`pgrep -af "codex exec"` 匹配到
  monitor 腳本自己（腳本文字含該字串）→ 永遠不判定結束；
  `for p in $(pgrep -f "wt-m5-9"); do kill $p` 匹配到**發出指令的 bash 自己** → 自殺。
  **一律 `pgrep -x <exact>` + 讀 `/proc/<pid>/cmdline`，不要 `pgrep -f` 加字串。**
- ⚠ **寄信建議要防抖 + 排除使用者自己的程式**。單次取樣會被 `7z` 解壓那種短命爆量誤觸；
  使用者打遊戲（`main` 537%）也會湊滿次數。**建議只是建議，先查證歸屬再決定寄不寄。**
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
