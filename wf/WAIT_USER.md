# WAIT_USER — 等待使用者的事（只列 open）

← [AGENTS.md](../AGENTS.md)｜[INDEX](INDEX.md)｜我的進度另記 [SESSION-LOG](SESSION-LOG.md)

需要**使用者親自做 / 驗證 / 決定**才能繼續的事——例如：實機/實環境測試、外部服務登入、環境變數設定、權限操作、需要帳號的下載、**催/開/fork 另一個 agent 處理急件**（見 [inbox](workflows/inbox/README.md)：寄了信但很急、對方可能沒開）。agent 做到結構性驗證＋打包的極限；跨不過去的那一關記這裡等使用者。

條目格式：`- [工作流] 請使用者做什麼 → 做完我接著做什麼`，**做完即刪**（不留已完成清單，歷史看 git log）。等別的 session / agent 不記這裡（判別表見 [WORKFLOWS](WORKFLOWS.md)）。

> **膨脹就拆**：堆多了就開 **`wait-user/`** 按類別拆檔，本檔只留一張 `| 類別 | open | 清單 |` 導航表，每列連到 `wait-user/<類別>.md`（照 [STRUCTURE](STRUCTURE.md)）。

## 待使用者項

### M9 呈現：素材本體只有你能做

管線我派得出去，**素材不行**——它需要你的生成模型與品味判斷。

- ⚠ **色盤的實際色值（48～64 色）＋ 10～20 張參考圖組**。
  [art-pipeline.md](../design/presentation/art-pipeline.md) 說這是「閘一的第一件事」、
  「七百張圖的地基」，而且**它明講第 1 步不要急**。
  M9.2 的管線把色盤做成**輸入檔**（附確定性生成的佔位色盤），所以它不擋工程——
  但真色盤沒定之前，任何素材都不該開始生成，否則整批要重做。
- ⚠ **prompt 模板的前後綴＋反向 prompt**，以及**選哪個生成模型**。
  模板定案後「一個字都不准改」，要改就整批重生成。
- ⚠ **音訊的調性中心、配器組、LUFS 目標**（[audio.md](../design/presentation/audio.md) 閘一）。
  同理：M9.1 用佔位音，真素材等你。

### 需要你裁定的一個設計問題

- ⚠ **M9 要不要碰 observer mark？** M6.6 實作者判定湧現任務
  「比殺 10 隻狼好，但只有半步意義，不算有靈魂」，診斷是缺 **mark**、歷史因果、
  具名後果。但 mark／[unique-objects.md](../design/simulation/unique-objects.md) 是你裁定**擱置**的，
  [milestones.md](../design/milestones.md) 寫著「要做之前先問」。
  **不解除擱置的話，「湧現任務有靈魂」這條要留到 M9 之後。**

### 只有你能給的判準

- ⚠ **M9 的第一條判準有一半要人來看**：「關掉 core 重開，畫面與音景完全一致」。
  畫面那半 M8.2 已經有機器證據（四層 free 重建 ImageMagick `AE = 0`）；
  **音景那半沒有等價的機器度量**——最後還是得你戴耳機聽切層有沒有穿幫。
