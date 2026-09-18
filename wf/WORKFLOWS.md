# WORKFLOWS — 工作流派發器

← [AGENTS.md](../AGENTS.md)｜專案地圖 [INDEX.md](INDEX.md)｜結構 [STRUCTURE](STRUCTURE.md)

你（使用者）說要做某件事 → **從這張表選對應工作流 → 讀它的「入口檔」→ 就知道要做什麼**。每個工作流的細節都在它自己的入口檔，不在這裡。

**可以跳流程**：單行或小範圍、低風險、不跨 session 的修正；純查詢或一次性回答，不留 durable 知識；使用者明確要求快速處理；既有工作流只會增加同步成本而不降低風險。跳流程不等於跳過工程規矩——仍要讀必要上下文、不破壞使用者改動、能測就測。

## 你想做什麼 → 用哪個工作流

### 開發 flavor

碰原始碼的工作流共用 [common/conventions](workflows/common/conventions.md)（寫碼慣例）與 [common/code-map](workflows/common/code-map.md)（哪個檔負責什麼）。

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「我想開發 / 修改某個功能」「**修 bug**」 | **feature-dev** | [workflows/feature-dev/README.md](workflows/feature-dev/README.md) |
| 「跑測試 / 驗證」「這樣改有沒有壞」 | **testing** | [workflows/testing.md](workflows/testing.md) |
| 「**拆檔 / 重構 / 整理結構**」（行為不變）| **refactor** | [workflows/refactor/README.md](workflows/refactor/README.md) |
| 「搬檔案 / 改目錄名 / 拆 repo」 | **refactor**（搬移專章）| [workflows/refactor/moving-things.md](workflows/refactor/moving-things.md) |
| 「**看看現在狀況**」「**這個功能有做嗎 / 差距在哪**」「解讀外部系統 / 可行性」 | **investigation** | [workflows/investigation/README.md](workflows/investigation/README.md) |
| 「規劃 spec」「把 roadmap 展開成規則／設計方案」 | **spec** | [workflows/spec.md](workflows/spec.md) |
| 「**開始規劃**」「把方案展開成動工計畫 / 拆輪次排依賴」 | **plan** | [workflows/plan/README.md](workflows/plan/README.md) |

**規劃管線**（一個想法的成熟過程）：investigation（現況如何？）→ spec（討論後方案）→ plan（動工前詳規）→ 任務書進 `inbox/` → build（feature-dev）。本專案用 spec／plan 取代 kernel 的 planning 管線；遊戲設計裁定記在 `design/` 各領域文件與 plan 的 `*-decisions.md`，工作流／團隊層面的決定才記 [decisions](workflows/decisions.md)。

### multi-agent flavor

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「看看信箱」「處理 inbox」 | **inbox（收信）** | [workflows/inbox/README.md](workflows/inbox/README.md) |
| 「寫任務書給 codex」「回報做完了 / 卡住了」 | **inbox（寄信）** | [workflows/inbox/README.md](workflows/inbox/README.md) |
| 「派工」「開一波」「怎麼驗收回報」「限流 / 並行 worktree」 | **dispatch** | [workflows/dispatch/README.md](workflows/dispatch/README.md) |
| 「要開團隊」「這層派哪級模型」「怎麼分角色」 | **team-model** | [workflows/team-model.md](workflows/team-model.md) |

### 定期喚醒 flavor

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「跑一次心跳」（多半由 [`/wf-tick`](../.claude/commands/wf-tick.md) 循環喚醒，不用人說）| **tick** | [workflows/tick.md](workflows/tick.md) |
| 「幫我加個常規事務」「登記例行」「每 N 天要做一次…」 | **routines** | [workflows/routines.md](workflows/routines.md) |
| 「幫我登記行程：17:00 重啟 X」「今晚 8 點提醒我 OO」 | **schedule** | [workflows/schedule.md](workflows/schedule.md) |

**tick 只派發、不判斷**；「什麼時間該做什麼」的判斷與清單各歸 routines / schedule。登記類請求只寫進清單、不當場做（等 tick 到點才做）。

### kernel 內建

| 觸發（你說…）| 工作流 | 入口檔 |
|--------------|--------|--------|
| 「記 / 查踩坑」 | **gotchas** | [workflows/common/gotchas.md](workflows/common/gotchas.md)（會咬人的專案陷阱在 [KNOWN-TRAPS](KNOWN-TRAPS.md)）|
| 「整理 X」「封存過時的」「檔案太多／太雜」「太大要拆」 | **tidy** | [workflows/tidy/README.md](workflows/tidy/README.md) |
| 「記個決定」「為什麼選 A 不選 B」（工作流／團隊層面）| **decisions** | [workflows/decisions.md](workflows/decisions.md) |
| 「我的偏好是…」「以後直接做 / 先問」 | **user** | [workflows/common/user.md](workflows/common/user.md) |

**都不符 → 看 [INDEX.md](INDEX.md)**。新開工作流 → 複製 [workflows/TEMPLATE.workflow.md](workflows/TEMPLATE.workflow.md) 並在上表加一列；工作流的統一形式與成長軌跡見 [STRUCTURE](STRUCTURE.md)。

## 活狀態記哪裡（只列 open，完成即刪）

| 在等誰 | 記哪裡 |
|--------|--------|
| 等**使用者**做 / 驗證 / 決定 | [WAIT_USER.md](WAIT_USER.md) |
| 等**同 repo 另一個 session / 角色** | [SESSION-LOG.md](SESSION-LOG.md) 一行 open（規劃者／實作者各寫自己的區塊）|
| 等**另一個 agent 做完一封任務書** | 信件軸：`inbox/` 頂層＝未辦、`done/`＝已辦，見 [workflows/inbox/](workflows/inbox/README.md) |
