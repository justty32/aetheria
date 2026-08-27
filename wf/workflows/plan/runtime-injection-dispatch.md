# M10 派工佈局 — 並行線、領地、版本號

← [runtime-injection.md](runtime-injection.md)（總覽）｜通用規矩：[OPS-NOTES](../../OPS-NOTES.md)、[CODEX-PROTOCOL](../../CODEX-PROTOCOL.md)

> 通用的 codex 指令、旗標坑、限流、即時通道**都在 OPS-NOTES／CODEX-PROTOCOL，不重抄**。
> 本檔只放 M10 特有的：哪些輪可以並行、各線領地、版本號誰動。

## 波次並行圖

```
波0  R0a 原則五守門測試 ──┐
     R0b 雜湊改關係斷言 ──┼─ 三線並行（worktree × 3）
     R0c 遊戲會存讀檔  ──┘
波1  R1 世界檔自帶 raws ──→ R2 角色檔             （序列化重地，單線串行）
波2  R3a 歷史日誌+結算協調器 ──→ R3b def 注入 ──→ R4 勢力可增長（單線串行）
波3  R5a 兵種資料化 ‖ R5c AI 目標動作資料化        （兩線並行）
     R5b 建築型別資料化＋R5c 的 goal codec 變更（都排進波3 的整合輪）
波4  R6a 人物實體 ‖ R6b 文明/文化 def ‖ R6c 物品   （三線並行）
波5  R7 注入入口（等裁定#2）──→ R8 端到端整合驗收
```

- 每波結束開一個 **INT 整合輪**（沿用 `m*-int-*` 慣例）：合併、解衝突、統一定版、重跑全綠。
- 研究型任務（讀碼出報告、不寫程式）不占線，隨時可加派。

## 領地表（照 skyrim `line-claims` 的做法：開工前宣告，寫進任務書）

| 線 | 可寫 | 明令不可碰 |
|---|---|---|
| R0a | `tests/`、`tools/`（新腳本）、CMake 的 tests 區段 | `core/` 全部 |
| R0b | `tests/` 既有雜湊測試檔 | `core/`；與 R0a 不同檔，任務書互列檔名 |
| R0c | `core/runtime/`、`bridge/`、`godot/` | `core/serialize/`、`core/zone/` 格式、版本號 |
| R1/R2 | `core/serialize/`、`core/zone/`、`core/rules/`（載入路徑）、`sim/` | 同波無他線 |
| R3a/3b/R4 | `core/world/`、`core/runtime/`、`core/serialize/`、`core/rules/`（patcher）、`sim/` | — |
| R5a | `core/site/site_combat*`、`core/rules/`（新 loader 檔＋`ruleset.{h,cpp}` **尾端追加**）、`data/units.toml`、相關測試 | `core/serialize/`、`core/ai/` |
| R5c | `core/ai/`、`core/rules/`（同上，尾端追加）、`data/`（AI 目標動作 toml）、相關測試 | `core/site/`、`core/serialize/`（goal codec 歸整合輪） |
| R6a/b/c | 各自新檔為主；新增 `.cpp` 進 CMake **只准一線做**，其餘兩線在信裡列出讓整合輪加 | 互不碰對方的 `data/*.toml` |

## 版本號政策（撞過兩次，鐵律）

- `kSaveFormatVersion` **只有整合輪可以動**，每波至多 +1（波內所有格式變更打包成一次 bump）。
- 並行線要改格式：改在自己 worktree、**不改版本號**、在回報信裡列出「我動了格式的哪裡」，
  整合輪彙總後統一定版。
- 預期軌跡：波1 → v22（raws＋角色檔；角色檔另有**獨立的 `kCharacterFormatVersion`**）、
  波2 → v23（歷史日誌＋勢力表）、波3 → v24（建築字串 id＋兵種＋goal codec）、
  波4 → **v25 確定**（AllComponents 必動）。
- ⚠ R5a/R5c 同時往 `ruleset.{h,cpp}` 尾端追加：整合輪解衝突照 OPS-NOTES 的
  「被切開的括號」坑處理（宣告區直接串接、定義區要補 `}`）。

## 每份任務書的固定配備（缺一不發）

1. **不要做的事**表格（硬邊界逐條，含上面的領地）。
2. **驗收條數寫死**（skyrim 實測：不寫死它會為保險亂跑驗證燒 token）。
3. 明令**不准 fan-out 自我審查**、**不准為數字好看反覆試值**（試了幾個值如實寫進回信）。
4. 那句最值錢的話：「發現 X 可被 Y 取代／誤差變大／規則一次沒命中，**如實回報**」。
5. 回報信落 `wf/inbox/`（`m10-*-complete.md`），**標題是一句自足的結論**；
   中途用 `.ask`（附假設、不停）／`.blocked`（真的停）——見 CODEX-PROTOCOL。

## 借自 skyrim inbox 工作流的三條（其餘我們已有等價物）

- **終局狀態語意收斂成四種**：回報信第一行標 `DONE`／`BLOCKED`／`NEEDS-USER`／`FAILED`。
  `NEEDS-USER` ≠ `BLOCKED`：前者是只有使用者能決定的事，直接轉登 [WAIT_USER](../../WAIT_USER.md)。
- **卡住超過 10 分鐘必須發訊**，不可無聲等待（寫 `.ask` 或 `.blocked`）。
- **上線先聲明領地**：codex 開工第一步在 `.progress` 裡覆述自己的可寫路徑——
  覆述錯了能在第一分鐘抓到，而不是在合併時。
