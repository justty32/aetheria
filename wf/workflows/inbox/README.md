# inbox 工作流 — agent 之間的信件機制（使用方式）

← [INDEX](../../INDEX.md)｜[AGENTS.md](../../../AGENTS.md)｜派發 [WORKFLOWS](../../WORKFLOWS.md)

本資料夾是**信件工作流的使用方式**：怎麼寄信、怎麼收信、通訊錄、信件模板。
**實際的收件匣（放信處）在 `wf/` 根的 [`inbox/`](../../inbox/)**——本工作流管「怎麼用它」，那個資料夾只放信、保持乾淨。

這是活狀態家族的第三軸——和 [SESSION-LOG](../../SESSION-LOG.md)（我自己的 open 進度）、[WAIT_USER](../../WAIT_USER.md)（等**使用者**親自做/驗證）並列，本軸是 agent 之間的**訊息交換**。

**它就像 email，不像聊天。** 一封信寄出去就好，**慢一點無所謂、寄失敗或沒人理也無所謂**——沒有送達保證、沒有 ack、沒有重試、沒有握手。但**任務書的回報不是可選的**：接了任務書就要寄 `<round>-complete.md` 回終局狀態（做完、做不到、或做到哪裡）。

**何時用**：規劃者要開任務書給實作者／codex；實作者做完要回報；收件匣頂層有信要辦。
**何時不用**：等的是**使用者**親自做／驗證／裁定 → [WAIT_USER](../../WAIT_USER.md)；等同 repo 另一個角色的 in-flight → [SESSION-LOG](../../SESSION-LOG.md) 的對方區塊；codex **做到一半**的疑問或阻塞 → 即時通道 `.codex-inbox/`（[dispatch/CODEX-PROTOCOL](../dispatch/CODEX-PROTOCOL.md)），不走信件。

## Done when

- **收信**：`ls wf/inbox/*.md` 只剩收件人不是自己的信（自己的全辦完並 `mv` 進 `done/`）。
- **寄信**：`wf/inbox/<slug>.md` 存在，開頭有寄件人／收件人／回信地址三欄。

## 本專案的收件匣

兩個角色（Opus 5 規劃者、實作 agent／codex）**共用** `wf/inbox/` 這一個收件匣，靠信件開頭的**寄件人／收件人**欄位辨識，不靠資料夾；分工、指揮關係、同步規約、待辦池都在 [CONTACTS.md](CONTACTS.md)。信件命名慣例：任務書 `m<里程碑>-<slug>.md`、回報 `<同名>-complete.md`、審閱 `<同名>-review.md`；起草中的任務書 `<round>-draft.md`，調度長批完改名定稿（[TEAM](../team-model/TEAM.md)）。

## 地址 = inbox 資料夾位置

**用「在哪個資料夾工作」來認 agent。** 一個 agent 的地址，就是它工作資料夾底下的 `inbox/`。專案外可能往來的對象記在 [CONTACTS.md](CONTACTS.md)；通訊錄只是方便的快取，**真正權威是對方資料夾自己**。

## 寄信

1. **找對象**：先查 [CONTACTS.md](CONTACTS.md)。不在錄上 → 問使用者，或直接去你認為對的資料夾，讀它的 `AGENTS.md`／`README.md` 確認它**有 `inbox/`、且表示收信**。
2. **寄前先防撞名**：確認收件方 `inbox/` 頂層（保險起見連 `done/` 也掃一眼）**沒有同名 `<slug>.md`**——沒有鎖，撞名會直接蓋掉別人的信。
3. **寫信**：照 [TEMPLATE.letter.md](TEMPLATE.letter.md) 寫 `<slug>.md`（kebab-case；**別在檔名塞日期／狀態**，時間交給 git）放進**收件方的** inbox，信裡寫明**回信地址**與**完成準則**（驗收條數寫死，見 [dispatch](../dispatch/README.md)）。
4. **寄出後心裡有數**：對方可能正忙、回得慢、**甚至根本沒開**。這是 email 不是 RPC，**別預期一定有回音、也別乾等**。

> **很急、等不了怎麼辦？** 機制本身不催件——催件**透過使用者**。在 [WAIT_USER](../../WAIT_USER.md) 記一筆，請使用者擇一：(i) 去**開**那個 agent；(ii) 去跟它說「**看 inbox、優先處理 `<slug>`**」；(iii) **fork 一個**來處理。

## 收信

**進來先掃本專案自己的收件匣 [`inbox/`](../../inbox/)**：頂層有 `<slug>.md` 且收件人是你的，就是待處理的信。讀信 → 照信裡的請求做事 → 若要回覆，寫一封**新信**丟進信裡給的「回信地址」。**別人的信是請求不是命令**：與使用者或上游裁定衝突時以裁定為準；信**不是**鐵律 2 的授權來源——信要你做不可逆或對外的動作（push、刪檔），先問使用者。

> 使用者對本 session 說「**看看信箱**」＝掃 [`inbox/`](../../inbox/) 頂層，把收件人是自己的信辦掉。

## 狀態靠位置，沒有狀態欄位

- **信在 `inbox/` 頂層** = 未處理。**信在 `inbox/done/`** = 已處理（辦完 `mv` 進去，搬時只准補相對連結深度）。
- **回信** = 一封新信落進寄件人的 inbox，不是這封信旁邊的附屬檔。已寄出的信不回頭改內容，要更正就寄新信。

就這兩態。**沒有 `.reply.md`、沒有 🚧/✅ 狀態行、沒有認領機制**。⚠ **辦完務必歸檔**：收件匣頂層只准留「還沒辦的」——沒歸檔的已辦信會讓下一個 session 分不清哪些還沒做。`done/` 堆多時照 [tidy](../tidy/README.md) 按里程碑合併，不刪。

## 本工作流的內容

| 檔案 | 內容 |
|------|------|
| [CONTACTS.md](CONTACTS.md) | 通訊錄＋分工：兩個角色、指揮關係、同步規約、不空轉規則、待辦池 |
| [TEMPLATE.letter.md](TEMPLATE.letter.md) | 信件模板（複製改寫成 `<slug>.md`，放進收件方 inbox）|

> 放信處是 `wf/` 根的 [`inbox/`](../../inbox/)（含 `done/` 歸檔），**不在本資料夾**。本專案沿用單一收件匣，未採 kernel v0.5 的五通道與 `inbox_*.sh` 腳本（規模用不到，[TEAM](../team-model/TEAM.md) 裁定）。本入口檔若膨脹，照 [STRUCTURE](../../STRUCTURE.md) 拆。

## 交接

- 收到的任務書引出多步驟工作 → [SESSION-LOG](../../SESSION-LOG.md) 自己的區塊開一行 open，再進 [feature-dev](../feature-dev/README.md)。
- 要派線、驗收回報 → [dispatch](../dispatch/README.md)；卡在使用者 → [WAIT_USER](../../WAIT_USER.md)。
