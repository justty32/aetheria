# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](../AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log（若有「已落地功能目錄」則濃縮一句進去）。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾，按工作流／類別**拆檔 + 一個 index 導航**（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

本檔同時 ① 連到各工作流自己的 session-log（若該工作流已長出自己的），② 收**不屬任何工作流**的進度。

> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**（設計決策/修了什麼落到該工作流的文件、待使用者驗的進 [WAIT_USER](WAIT_USER.md)）。完成即整條刪除。

## 最新進度

- **M0.1 任務書已寄出** → `wf/inbox/m0_1-hardening.md`（審閱結果在 `m0-review.md`），
  等 gpt-sol 實作者接手。M0 已提交（`ab38ce1`）。
- ⚠ **M0 的三個守衛還不是真的**：核心隔離的 CMake 檢查只守它上面的行、
  `to_date` 對超域 Tick 靜默溢位、`AETH_CHECK` 尚未建立。修法在 M0.1 任務書。
- **M0.5（Zone／ZoneManager）任務書還沒寫**，要等 M0.1 驗收。前置：
  [design/zone-model.md](../design/zone-model.md) 已 8,159 bytes（上限 8 KB 只剩 33），
  **寫 M0.5 之前得先拆它**。[design/outline.md](../design/outline.md) 同樣逼近上限。
- ⚠ **已知設計缺口**：同層近距離事件的快速路徑（兩個參與者分屬相鄰 Local zone 的戰鬥），
  `events.md` 沒寫清楚，刻意留到實作撞到再補——現在硬定形狀很可能定錯。
  記在 [lowmap-streaming.md](../design/lowmap-streaming.md) 末段。
- **刻意擱置**：mark 與獨特物件的細節；root zone 的成長軸（使用者裁定：過早優化，後面再說）。
- ⚠ **實作時要注意的混淆點**：「戰鬥位階」與「聚合提升重要性」共用 significance 等級表，
  但**升級規則不同**——人多會提升「被個別計算的資格」，不會提升戰鬥位階。見 power-tiers.md 末段。
- **未規劃但已知的缺口**：垂直層玩法、美術資源工作流。
  完整清單見 [design/README.md](../design/README.md) 的「尚未規劃」。

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何工作流的進度

- （無）
