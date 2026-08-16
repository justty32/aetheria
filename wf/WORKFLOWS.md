# WORKFLOWS — 工作流派發器

← [AGENTS.md](../AGENTS.md)｜專案地圖 [INDEX.md](INDEX.md)

你（使用者）說要做某件事 → **從這張表選對應工作流 → 讀它的「入口檔」→ 就知道要做什麼**。每個工作流的細節都在它自己的入口檔，不在這裡。

## 你想做什麼 → 用哪個工作流

### 開發 flavor

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「我想開發 / 修改某個功能」「**修 bug**」 | **feature-dev** | [workflows/feature-dev/README.md](workflows/feature-dev/README.md) |
| 「**拆檔 / 重構 / 整理結構**」 | **refactor** | [workflows/refactor.md](workflows/refactor.md) |
| 「跑測試 / 驗證」 | **testing** | [workflows/testing.md](workflows/testing.md) |
| 「**記 / 查踩坑**」 | **gotchas** | [workflows/common/gotchas.md](workflows/common/gotchas.md) |

碰原始碼的工作流共用 [common/conventions](workflows/common/conventions.md)（程式碼慣例 + code map）。

> 以下是常見**開發類**工作流菜單，**需要哪個才加哪列**（入口檔在第一次用到時才建，從單檔開始長——見 [DEV-GUIDE](DEV-GUIDE.md) 四級成長軌跡）：
> - **investigation**（調查 / 解讀外部系統 / 可行性研究）
> - **spec**（把一個 idea 討論成設計方案）
> - **plan**（把設計方案展開成動工計畫）
> - **idea**（記一個奇思妙想，不確定要不要做）
> - **roadmap**（記一件確定會做、不確定何時的事）
> - **tooling**（外部工具設定 / env var / 依賴）
> - **dev-env**（開發環境、fresh clone 後要做什麼、打包出貨）
>
> 若採用規劃類工作流，建議保留這條**規劃管線**（一個想法的成熟過程）：
> idea（要不要做？）→ roadmap（會做，何時？）→ spec（討論後方案）→ plan（動工前詳規）→ build（feature-dev）。

**都不符 → 看 [INDEX.md](INDEX.md)**（repo 頂層結構地圖）。

## 定期喚醒（kernel 內建，與上面 flavor 派發表分開）

一套定期工作流，**不屬任一 flavor、kernel 一律有**。**兩種進入**：
1. **循環執行**：[`/wf-tick`](../.claude/commands/wf-tick.md) 每隔週期喚醒 tick → tick 派發下面各工作流，判時間、**做**到期項。
2. **使用者登記**：你直接請求 →「**幫我登記行程**」進 schedule、「**加個常規事務**」進 routines，只寫進清單、不當場做（等 tick 到點才做）。

| 工作流 | 入口 | 做什麼 |
|--------|------|--------|
| **tick** | [workflows/tick.md](workflows/tick.md) | 定期心跳的**單次**執行——當**派發器**依序跑各定期工作流（routines → schedule）。由 `/wf-tick [週期]` 每隔週期喚醒。 |
| **routines** | [workflows/routines.md](workflows/routines.md) | **固定例行**清單（不常變動）：判當地時間 → 對照時機分區 / 間隔登記表 → 到期的唯讀事務就做。 |
| **schedule** | [workflows/schedule.md](workflows/schedule.md) | **一次性**定時請求（心血來潮，如「17:00 重啟 XXX」）：判時間 → 到點的就做、做完刪。 |

**tick 只派發、不判斷**；「什麼時間該做什麼」的判斷與清單各歸 routines / schedule。整套用不到就把 `tick` 連同 [`/wf-tick`](../.claude/commands/wf-tick.md) 一起刪。

## 工作流的統一形式（規範）

所有工作流照同一套形式（細則見 [DEV-GUIDE](DEV-GUIDE.md)）：

**檔名規範**：
- **README** = 初入一個資料夾**先讀的入口／導引**（這資料夾在幹嘛、怎麼用）。
- **INDEX** = **描述該資料夾頂層結構**的索引（有哪些子項、各放什麼）。
- 小資料夾兩者可合一（README 兼述結構）；大到結構複雜時才分出獨立 INDEX。

形式：
- **資料夾型工作流**：一個**入口 README**（或主檔）+ 視需要的 `archive/`（過時/被取代文檔封存）、`gotchas.md`（踩坑）、`session-log.md`（本工作流 open 進度）。
- **單檔工作流**：一個 `.md` 同時是入口與內容；撐大了照「[結構整理原則](DEV-GUIDE.md)」升級成資料夾型。
- 入口檔本身膨脹 → 一樣照結構整理原則拆。
- 非微小工作先寫 `Done when:`，完成定義要可驗證，並說清楚不包含什麼。

## 跨工作流的活狀態（repo 根）

三軸各管一種「還沒完成的事」，都**只列 open**、完成即移除：

- **進度**（我自己還沒完成的 in-flight / open）→ [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者親自做 / 驗證的**（實機環境 / 外部工具 / env / 權限）→ [WAIT_USER.md](WAIT_USER.md)
- **信件**（agent 之間的訊息交換，像 email——寄失敗/不回都無妨；放信處是 `wf/inbox/`）→ 使用方式見 [workflows/inbox/](workflows/inbox/README.md)
