# SESSION-LOG — 進度日誌（hub，只列 open）

← [AGENTS.md](../AGENTS.md)｜[INDEX](INDEX.md)｜等使用者的另記 [WAIT_USER](WAIT_USER.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log 與 `inbox/done/`，收線判斷濃縮進 [feature-dev/landed](workflows/feature-dev/landed/README.md)，會咬人的進 [KNOWN-TRAPS](KNOWN-TRAPS.md)。

**寫入時機**：① **開始**多步驟工作前先寫一行 open（不是做完才寫）；② 每次 commit 後更新或刪除該行；③ 條目格式 `- [工作流] 一句 open 狀態 → 下一步 / 連結`，完成即刪。

> **膨脹就拆**：過大就開 **`session_logs/`** 按工作流拆檔＋index，本檔只留導航（照 [STRUCTURE](STRUCTURE.md)）。

> **分區塊擁有**：`### 規劃者` 與 `### 實作者` **各自只寫自己那塊，永遠不碰對方那塊**——
> 連順手整理都不行。跨區的事寫信。規約見
> [workflows/inbox/CONTACTS.md](workflows/inbox/CONTACTS.md) 的「同步規約」。
> **in-flight 一定要有記錄**：session 隨時會斷，本檔是唯一交接面。

## 最新進度

> **2026-09-09 使用者宣布專案解除凍結，先做完整世界規劃，不開始實作**。
> 目前方向與文件入口：[三層奇幻活世界 roadmap](workflows/plan/living-world/roadmap-2026-09.md)、
> [S0–S11 規格草案](workflows/spec.md)。舊實作停在 M10.3a（v23、432 綠）。
> 恢復實作前先讀 [波 2 交接](workflows/plan/runtime-injection-wave2-handoff.md)；
> 建置環境的 configure 配方見 [OPS-NOTES](workflows/dispatch/OPS-NOTES.md)。

### 實作者（gpt-sol）

- （目前無任務書）

### 規劃者（Opus 5）

<!-- wf-nav -->
- [plan] 波 2 暫停於 M10.3a（已併入 main、**仍 v23 未 bump**）。待續 **M10.3b → M10.4 → v24 定版輪**，
  順序等 roadmap 重排裁定 → [波 2 交接](workflows/plan/runtime-injection-wave2-handoff.md)
  （v24 格式清單／繼承缺陷／多角色槽重放對不上）。
- [inbox] [M9.0 主線通關](inbox/m9-0-mainline-victory.md)與 [M9.1 音景](inbox/m9-1-soundscape.md)兩份任務書**仍在收件匣、未開工**
  （無 `-complete`、無 commit；2026-09-18 調度長裁定留頂層）；對應 [roadmap-coverage](workflows/plan/living-world/roadmap-coverage.md) 的 R5／R6，開工順序等 roadmap 重排。
- [investigation] 地形生成已交接、**尚無人接手**（濕度削頂根因已量到）→ [TERRAIN-HANDOFF](workflows/investigation/TERRAIN-HANDOFF.md)；
  活世界規劃以它為事實基線。
- [feature-dev] 「能動但不像真的」（M5 四處、M6.6 湧現任務只有半步意義）仍未解，卡在 observer mark 的擱置裁定 → [WAIT_USER](WAIT_USER.md)、[M5-CLOSEOUT](workflows/feature-dev/landed/M5-CLOSEOUT.md)。

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何工作流的進度

- （無）
