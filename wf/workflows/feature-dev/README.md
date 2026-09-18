# 功能開發（feature-dev）— 工作流入口

← [INDEX](../../INDEX.md)｜[AGENTS.md](../../../AGENTS.md)

新增 / 修改 aetheria 功能的工作流。這是本工作流的**入口**：先讀本檔，再往下深入。

**何時用**：使用者要開發／修改功能、修 bug；實作 agent 拿到 `inbox/` 的任務書要動 `core/`／`bridge/`／`godot/`。
**何時不用**：只查現況不改碼 → [investigation](../investigation/README.md)；行為不變的拆檔 → [refactor](../refactor/README.md)；方案還沒定 → [spec](../spec.md)／[plan](../plan/README.md)。
always-on 鐵律見 [AGENTS.md](../../../AGENTS.md)；要整理結構時參考 [STRUCTURE](../../STRUCTURE.md)（被動）；**程式碼慣例**見 [common/conventions](../common/conventions.md)。

## 流程

```
修改（增量）
  → 跑自動驗證（[testing](../testing.md) 的快速驗證；綠燈不等於有檢查，見該檔）綠燈
  → 交使用者驗證（若需實機/實環境）→ 回報問題 → 修 → 重複
  → 全數通過後：補齊 code map → 補文檔 → commit
```

- **自動驗證是你（Claude）自己跑**的把關（鐵律：改完跑驗證）。
- **Claude 跑不了的驗證一律由使用者做**——先靠自動驗證＋結構性檢查把握到極限再交付；需使用者驗證的記到 [WAIT_USER](../../WAIT_USER.md)。
- 測試迭代期間，code map / 文檔可暫時落後；**commit 前必須對齊**。
- 跨 session 時在本工作流 `session-log.md` 補一行 `[功能名] 文檔/code map 待同步`，下個 session 不會誤判已同步。

> 實際 build/test 指令見 [testing](../testing.md)，檔案導航見 [common/code-map](../common/code-map.md)。
> 驗收實作回報時**自己重跑負向控制**——為什麼不能省見 [dispatch/OPS-VERIFY](../dispatch/OPS-VERIFY.md)。

## 內容

| 檔案 | 內容 |
|------|------|
| [landed/](landed/README.md) | 已落地功能目錄（時間序）：M5 收線、M8.1 可玩迴圈等收線判斷 |
| `gotchas.md`（長出來才建）| 本工作流專屬踩坑 |
| `session-log.md`（長出來才建）| 本工作流 open / in-flight 進度（hub 在 [SESSION-LOG](../../SESSION-LOG.md)）|

> **archive**：過時/被取代的文檔封存進 `feature-dev/archive/`（保留歷史、不污染現役）。本入口檔若膨脹，照 [STRUCTURE「結構整理原則」](../../STRUCTURE.md) 拆。
