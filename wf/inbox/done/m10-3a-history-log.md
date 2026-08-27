# 任務書 M10.3a — 歷史日誌＋結算協調器

**寄件人**：規劃者（調度 session）｜**收件人**：gpt-sol 實作者
**必讀**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)、[計畫](../workflows/plan/runtime-injection-inject.md) M10.3a 節、
[裁定#4／#8](../workflows/plan/runtime-injection-decisions.md)、[KNOWN-TRAPS](../KNOWN-TRAPS.md) 最後一條
**工作分支**：`m10-3a`（worktree `../aetheria-wt-m10-3a`）
**前置**：波 0＋1 已併入 `main`（v23、423 測試綠、存讀檔、raws-in-save、角色檔）。
**波 2 單線串行**：3a → 3b → M10.4。

## 背景

世界不只由 seed 決定：移動排進 `RegionMoveCommand` 回合期結算（`playable_session.cpp:1085`），
戰鬥當場改 power 與 owner（`cpp:1179-1231`），16 個覆蓋命令各自改世界。
**只記注入無法重建世界——要記全部輸入。**兩個現況把重放擋死，本輪一起治：

1. **回合尾端沒有結算點**：`TurnEnd` 只是一行 notify（`region_turn.cpp:181`）；
   `queue_*` 有 `AETH_CHECK(in_tick_)`（`zone_manager.cpp:190,195,200`）且 session 回合路徑
   從不呼叫 `manager_->tick()`——**協調器要自己建，不是把 `queue_*` 掛上 TurnEnd**。
2. **亂數輸入不持久**（KNOWN-TRAPS）：`cpp:1176` 餵 `next_event_id_`／`seed_ + revision_`
   給戰鬥，兩者冷讀重置、`revision_` 被 17 處命令各 +1——軌跡依賴「這 process 做過什麼」。

## 要做的（7 件，順序建議照列）

1. **獨立日誌組件** `core/history/history_log.{h,cpp}`（新目錄）：append-only＋雜湊鏈＋重放介面。
   條目 = `{seq, tick, kind（**字串 id**）, payload（TOML 原文）, prev_hash, entry_hash}`，
   落 `saves/<世界>/history.log`；API 至少 `append`／`entries`／`head_hash`／`verify`（整鏈重算）。
   ⚠ **kind 一律字串 id，不准開 enum**——原則五守門（`tools/check_principle5.py`）會紅。
2. **記全部輸入**，記錄點放 **core**（`sim` 與測試都直接呼叫 session，bridge 不是唯一入口）：
   `issue_move`（`cpp:1077`，排隊型）、`advance_xun`（`cpp:1105`）、`resolve_encounter`
   （`cpp:1156`，立即結算型）＋**16 個三層覆蓋命令**（`playable_session.h:189-206`）。
   ⚠ 計畫寫的 `coverage_command` **不在 core**——它是 `bridge/aetheria_core.cpp:683-726` 的字串
   dispatcher；**沿用它那組字串當日誌 kind**，但記錄點放各 session 方法首行，靠私有
   `record_command(kind, payload)` 一處一行。
3. **結算協調器** `core/runtime/turn_commit.{h,cpp}`：world 級回合尾端 commit point。順序＝
   **日誌落盤 → commit marker → 依序套用 → zone/manifest 落盤**（末段沿用 `save_session`，
   `core/runtime/session_persistence.h:26`，已是 zone 先 manifest 後）。本輪**只處理玩家命令**；
   def 套用留空 hook，**不實作**（3b 的活）。
4. **崩潰恢復（journal-first）**：commit marker 落**獨立檔** `saves/<世界>/history.commit`
   （單一 u64＝已套用的最後 seq，tmp+rename 原子寫）。開槽時 `head_seq > commit_seq` → 自動重放尾巴。
5. **uid 統一走 manifest 配發器**。欄位在 `core/zone/save_manifest.h:23-24`（**不是**計畫寫的
   `file_zone_store.h`），**全 repo 沒有任何遞增處**（只有 `save_manifest_io.cpp:96,111` 與測試）
   ——是死欄位。本輪做成配發器；寫死 uid 全數退場：`playable_session.h:271-272`、`cpp:417`、
   `cpp:902`、`cpp:1200`。
6. **亂數輸入改成只依賴持久態**（關掉 KNOWN-TRAPS 那條）：`cpp:1176` 與 `cpp:1210` 的
   `next_event_id_`／`seed_ + revision_` 改由 **seed＋tick＋日誌 seq** 推導。`revision_` 可留作
   UI 髒標記，但**不得再進任何亂數輸入**。改法自選，形狀寫進回信。
7. **世界身分雜湊＋`sim replay`**：雜湊明定為 **zone 正規化雜湊 ⊕ raws 雜湊 ⊕ 日誌鏈頭**——現況
   `sim/world_hash.cpp:149-155` 只混 zone `.bin`，raws_hash 僅在 `:104-106` 檢查非零、**沒進雜湊**。
   `sim` 加 `replay <slot>`（比照 `verify world-hash` 在 `sim/main.cpp:89-98` 的形狀，
   **在載入全域 ruleset 之前分派**，不吃 `--data-dir`）：seed＋基底 raws＋日誌重建 → 比對雜湊。

**明確排除**：改寫 M9 判準與 `design/milestones.md`——規劃者自己改文件，**codex 不碰 `design/`**。

## 驗收（就這 6 條，不要多跑）

| # | 標準 |
|---|---|
| 1 | 玩 N 旬（含 `issue_move`／`advance_xun`／一次戰鬥／至少三個覆蓋命令）→ 存檔 → `aetheria_sim replay <slot>`：**現役／冷讀／重放三者世界身分雜湊相等**（附三筆輸出） |
| 2 | **負向控制**：篡改 `history.log` **中段**一個 byte → 開槽 fail-fast，訊息指出鏈斷在第幾筆；還原後綠。附兩次輸出 |
| 3 | **負向控制**：套用中途中止（測試鉤：日誌與 marker 已落、套用未跑）→ 重啟自動重放尾巴 → 雜湊與**不中斷**跑法**逐位元相等**。附兩次輸出 |
| 4 | **負向控制（本輪核心）**：**無任何命令**時（開新世界即存檔）與 v23 `main` 同 seed 世界雜湊**位元級全等**。附兩邊輸出 |
| 5 | 1001／2001／9001 在 `core/` 全數退場（附 grep 為空）；uid 改由 manifest 配發，存→讀→再配發不撞號（附序列） |
| 6 | **KNOWN-TRAPS 的關閉憑證**：「不中斷玩到第 N 旬的戰鬥」與「第 N−1 旬存檔冷讀後玩到同一場戰鬥」的 `layer_result` **逐欄位相等**；全套 `ctest` 綠（`--parallel 2`），總數不減（現 423） |

## 模組與依賴（原則九，回報必填）

- `core/history/history_log.{h,cpp}`——**payload 無關**：只准依賴 `core/base/check.h`、
  `core/time/` 與標準庫，其餘 core 子目錄與 bridge **一律不得 include**（驗收直接看清單）。
  打掉重做時最值得整組搬走的組件。
- `core/runtime/turn_commit.{h,cpp}`——世界專屬的 payload 解讀住這裡，依賴 history_log＋zone＋
  session_persistence。`PlayableSession` **只呼叫它**，日誌邏輯一行都不准長進 session（**只准變薄**）。
- 回報畫兩行清單：各自依賴誰、誰依賴它。

## 不要做的事

| 不要 | 理由 |
|---|---|
| `RulesetPatcher`、def 套用、影子驗證、指標 id 化 | M10.3b |
| `add_faction`、外交 codec、勢力 remap｜`inject/` 吸收入口 | M10.4｜M10.7（裁定#2） |
| bump `kSaveFormatVersion`（現 23，`core/serialize/zone_codec.h:13`） | **波 2 由整合輪打包 v24**。動了格式的地方**逐條列進回信** |
| 把 commit marker／日誌 seq 塞進 `SaveManifest` | 會逼出 bump；走獨立檔 |
| 新開任何 enum（含 journal kind） | 原則五守門會紅；一律字串 id |
| 日誌換 SQLite／DB｜截斷重放、原地改寫日誌 | 裁定#7｜裁定#4 append-only |
| 注入內容寫回 `<slot>/raws/`｜順手接 named fate 持久化 | 裁定#8｜M10.6a |
| build 超過 `-j2`、ctest 超過 `--parallel 2` | 同機可能另有一路在編譯 |
| push｜fan-out 子 agent 自審｜改 `design/` | 一律禁止 |

## 回報

`wf/inbox/m10-3a-history-log-complete.md`（≤8KB 繁中，標題一句自足結論，首行標
`DONE`/`BLOCKED`/`NEEDS-USER`/`FAILED`）：驗收 6 條逐項證據（三個負向控制前後輸出必附）、
動了格式的哪裡（逐檔列，給整合輪定 v24）、亂數改法最終形狀、模組與依賴節、全部被迫假設。
**不准為數字好看反覆試值**。中途疑問 `.codex-inbox/m10-3a.ask`（寫下假設繼續做）；卡 10 分鐘必發。

**最重要**：發現任何玩家輸入**無法**只用「seed＋基底 raws＋日誌」重建（依賴記憶體狀態、
filesystem 走訪順序、未持久化的計數器）——**如實列出**，別用「反正跑起來一樣」蒙混。
那份清單就是 3b 與 M10.4 會炸的地方，比功能本身更有價值。

---

（不 bump 論證：uid 欄位 v23 已存在、marker 與日誌是新獨立檔、manifest 零變更。）
