# 任務書 M9.0 — 主線劇情可跑完一條，且能通關

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**必讀協定**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)
**工作分支**：`m9-0-wt`（新 worktree，基準 = M8-INT-7 合併後的 main）
**設計依據**：[narrative.md](../../design/narrative.md)、[milestones.md](../../design/milestones.md)
的 M9 兩條判準、[zone-save-history.md](../../design/zone-save-history.md)

---

## 這一輪要達到的一句話

**從新遊戲開始，一條主線能被推到通關，而且同一存檔跑兩次、每個檢查點的世界雜湊相同。**

## ⚠ 不新增玩法。主線只是把既有機制串成一條可完成的線

M6／M7／M8 已經有外交締約、地城清空、Site 攻陷、湧現任務完成。
主線的每個階段**推進條件一律掛在這些既有機制上**，不准為主線發明新系統。
`narrative.md` 的賭注是「敘事是模擬出來的」——主線是**骨架**，不是劇本。

階段數自訂但**至少 4 個**，最後一階由既有的 victory 掛勾（M8.0 已有
`check -> Outcome`）判通關。

## ⚠ 最容易出事的一件：劇情進度不能存在 Lua 裡

M8.0 的**鐵律 4** 是每次掛勾新建 `_ENV`、存檔不含 Lua state
（`LuaDeterminismLaw4.FreshEnvironmentDropsCrossTurnStateAndSaveOmitsLuaState`）。
所以**主線階段必須是 core 的持久狀態**，經 `Context` 讀寫。

目前 `core/script/include/aetheria/script/context.h` 只開了
`owner` / `set_owner` / `rng()`。這一輪要擴充 `Context`，加上主線階段的受限讀寫。

⚠ **不准把 `RegionTiles` 或任何指標漏進 Lua**——維持 M8.0 的型別隔離，
`ScriptContextIsolation.WorldTruthCompileFailure` 那條雙向證據要照樣綠。

## ⚠ 存檔版本號：這一輪獨佔

主線階段要進存檔，所以 `kSaveFormatVersion` **20 → 21，只有這一路能動**。
（OPS-NOTES 記過：沒劃清楚，一晚撞了兩次版本號。）
若同期還有別輪在跑，它們一律不准動這個常數。

⚠ **缺席 ≠ 中性**（[zone-save-history.md](../../design/zone-save-history.md)）：
舊存檔沒有主線欄位時，**不要當成「階段 0」默默吃下去**，照既有慣例 fail-fast。

## 驗收（每一條都要在回報裡附數字）

| 項目 | 標準 |
|---|---|
| headless 通關 | `aetheria_sim` 能從新遊戲跑到 victory，exit 0，附用了幾旬 |
| **兩次通關雜湊逐點相同** | 同一存檔跑兩次，**每個階段推進點**各取一次世界雜湊，逐點相等，附全部數字 |
| **存檔往返不改變結局** | 中途存檔 → **冷載入**（新行程）→ 續跑到通關，與一路跑到底的雜湊**逐檢查點相同** |
| **每個階段都真的命中過** | 附各階段命中次數，**不得有 0**（空的階段必然通過，那不算覆蓋） |
| Godot 可見 | 主線當前階段與通關結果要能在既有面板看到，附 headless exit 0 |
| ctest | 全綠，附 N，且 **N 必須大於合併後 main 的基準**，說明新增了幾個 |
| 文件 | `find . -name '*.md' -size +8k` 為空 |

### 負向控制（一次只改一條，取得紅燈後還原）

| # | 注入 | 必須紅 |
|---|---|---|
| 1 | 把主線階段改成存在 Lua `_ENV`（違反鐵律 4） | 存檔往返或兩次通關的雜湊比對 |
| 2 | 讓某一個階段的推進條件永遠為真（跳過中間階段） | 「各階段命中次數不得有 0」或雜湊比對 |
| 3 | victory 掛勾回傳固定 `Outcome` | 通關判定測試 |

⚠ **無效注入不算通過**（[verification-detection-power.md](../../design/verification-detection-power.md)）：
若某條注入下來全綠，**如實寫進回報說這條測不到**，不要換一個更容易紅的注入來充數。

## 不要做的事

| 不要 | 理由 |
|---|---|
| 改 `design/` | 有設計異議寫進回報，我裁定 |
| 為主線新增玩法系統 | 主線是骨架，推進條件掛既有機制 |
| 碰 mark／獨特物件 | 使用者裁定擱置，要做之前先問 |
| 動美術或音效 | 那是 M9.1／M9.2，不是這輪 |
| push | 一律要使用者點頭 |
| fan-out 自我審查子 agent | 禁止 |

## 回報

`wf/inbox/m9-0-mainline-victory-complete.md`：
階段表與各階段命中次數、兩次通關的逐點雜湊、冷載入往返的逐點雜湊、
三條負向控制各自的紅燈輸出（或哪一條測不到）、`Context` 新增了什麼、
`kSaveFormatVersion` 21 的欄位、ctest N 的組成、現有測試證不了的事。

## 規約

- `cmake --build build --parallel 2`｜**build exit code 是 0 才准跑 ctest**｜
  不准 fan-out 子 agent｜不改 `design/`｜不 push｜繁體中文、每份文件 ≤ 8 KB
- 有疑問但猜得下去 → `.codex-inbox/m9-0.ask`：**寫下假設然後照假設做完，不要停**。
  真的做不下去 → `.codex-inbox/m9-0.blocked` 並停。完成 → `.codex-inbox/m9-0.done`。

## 最後一條，最重要

**如果你發現某個階段其實可以被別的東西取代、某條推進條件一次都沒命中、
某個雜湊比對其實比不到東西、或某項驗收你其實沒真的做到，如實回報，
不要硬湊一個情境糊過去。這比做完更有價值。**
