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

- 🔄 **M6.0 力量體系**（`m6-0-wt`）→ [任務書](inbox/m6-0-power-tiers.md)
- 🔄 **M6.1 個體規則**（`m6-1-wt`）→ [任務書](inbox/m6-1-individual-rules.md)
- 🔄 **M6.2 外交關係模型**（`m6-2-wt`）→ [任務書](inbox/m6-2-diplomacy.md)

### 規劃者（Opus 5）

> **完成的不留這裡。** M0～M2.3 的結論都在 git log 與 `design/` 裡；
> 下面只留**還沒完成、或會在未來咬人**的東西。

## 📌 下一步：M6 內容（戰鬥規則、勢力 AI、劇情）

**M0～M5 全部完成，272/272。** `outline.md` 的 M6 判準只寫了「戰鬥規則、勢力 AI、劇情」。

**M6 的設計文件早就寫好了**，不必重新設計，照著實作即可。輪次規劃：

| 輪 | 內容 | 設計文件 | 狀態 |
|---|---|---|---|
| M6.0 | 力量體系、等效戰力 `S`、階差門檻 | [power-tiers](../design/power-tiers.md) | 🔄 |
| M6.1 | 四屬性、d100、傷害抗性、`suggest_tier` | [rules-individual](../design/rules-individual.md) | 🔄 |
| M6.2 | 外交四分量、戰爭事件、厭戰、`FactionView` | [diplomacy](../design/diplomacy.md) | 🔄 |
| M6.3 | **Region 戰鬥公式**（吃 M6.0 的 `S`）＋具名參與者命運 | [combat-formula](../design/combat-formula.md)、[significance-fate](../design/significance-fate.md) | 待 |
| M6.4 | 勢力 AI：目標庫、效用評分、AI LOD 三級 | [faction-ai](../design/faction-ai.md) | 待 |
| M6.5 | 敘事：三種任務、事件模板、**Godot 顯示 UI**（M2 拖到現在） | [narrative](../design/narrative.md) | 待 |
| M6.6 | **三層校準**：E[Region]≈E[Site]≈E[Local]，相對誤差 <5% | [event-scaling](../design/event-scaling.md) | 待 |

⚠ **校準順序不可顛倒**：先把 Region 公式調到「打起來像那麼回事」，
再讓 Site 與 Local 去追它。反過來做會失控——低層自由度太高，拿它當基準等於沒有基準。

⚠ **M6 開場前要先處理的三件**（都是前面里程碑刻意留下的）：

1. **命運無偏測試需要具名 NPC** —— M4 記的，現在 M6 才有真的具名對象可測
2. **敘事事件已在 core 產生並可持久化，但 Godot 端沒有顯示 UI** —— 從 M2 拖到現在
3. ⚠ **M4 的 −1.29% 偏差要重量**。兩套人口公式當時都還是最小實作；
   有真實內容後它可能變大或**變號**，而**變號就是可刷的漏洞**
   （判準見 [interface-verification.md](../design/interface-verification.md) 的三條）

⚠ **M6 同時是「像不像真的」那條的判準來源**（見下節）——先有內容，才知道什麼叫像。

## ✅ M5 下層完成（272/272）

**獨立一檔** → [M5-CLOSEOUT.md](M5-CLOSEOUT.md)。
抽象檢驗的結果，以及**「能動但不像真的」**那條未解問題都在那裡。

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
