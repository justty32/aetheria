# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](../AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log（若有「已落地功能目錄」則濃縮一句進去）。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾，按工作流／類別**拆檔 + 一個 index 導航**（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

本檔同時 ① 連到各工作流自己的 session-log（若該工作流已長出自己的），② 收**不屬任何工作流**的進度。

> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**（設計決策/修了什麼落到該工作流的文件、待使用者驗的進 [WAIT_USER](WAIT_USER.md)）。完成即整條刪除。

## 最新進度

- **規劃階段進行中**：大綱與 L1／L2 玩法已細化（[design/README.md](../design/README.md) 有完整索引）。
  尚無任何程式碼、`project.godot`、CMake。
- **medps 繼承核對清單 11 條已全數完成** → [design/medps-relation.md](../design/medps-relation.md)。
  M0 的前置（`zone-model.md` / `zone-save.md`）已寫完；**zone 定址定案：混合方案**
  （空間 zone 座標推導、Detached 用序號），刻意不同於 medps 的零語意序號，理由在文件裡。
- **程序生成已細化完成**（6 份）：接邊採「降維裁決鏈」（角 → 邊 → 面），
  Site 永不寫自己的邊界，因此生成順序無關、缺席無關、重生成無關。可直接遞迴到 L2→L3。
- **事件系統已細化完成**（3 份 + `principles.md` + `player-residence.md`）：
  事件是跨層物件，主場層由 observer 決定；跨 Site 影響一律經 Region 中介；
  期望值一致 + δ 上界讓「要不要親自去打」成為有數學保證的權衡。
- **時間單位已改**：分鐘 → **秒**，且回合 stride 可變（交戰時 Site 900 秒、Local 6 秒）。
  理由是推演移動速度時發現原本的粒度會讓戰場機動失去意義，見 `design/combat-scaling.md`。
- **觀察點結構已修正**：不是多個平行來源，而是**單一根（玩家）+ 子觀察點樹**。
  子觀察點同時是**獨特物件的登記處**（[design/unique-objects.md](../design/unique-objects.md)）。
- **戰鬥公式與力量體系已簡單規劃**（[power-tiers.md](../design/power-tiers.md)、
  [combat-formula.md](../design/combat-formula.md)）：**量與質分開**，位階直接沿用 significance 五級、
  每階 ×4 折算成等效戰力；再加「階差 ≥ 3 幾乎免疫」的門檻，讓凡人對抗傳奇要靠**手段**而非人海。
  Region 公式用 `R^p`（p 初值 1.3）+ 崩潰閾值，**形式已定、數值全部待校準**。
- **L3 已規劃完成**（[lowmap.md](../design/lowmap.md)、[lowmap-streaming.md](../design/lowmap-streaming.md)、
  [localgen.md](../design/localgen.md)）：**三層都規劃完了**，且 L2↔L3 完全複用 L1↔L2 的機制，
  沒有引入任何新機制。串流＝場強換一組參數；跨 zone 規則精確化為
  「**看可以跨、改必須經中介**」，且生成期絕不跨。
- **協作模式已建立**（使用者 2026-08-15 訂定）：規劃者寫設計與任務書、不寫實作；
  實作 agent 依 `wf/inbox/` 的任務書實作、不自行改設計。兩者共用收件匣，
  見 [workflows/inbox/CONTACTS.md](workflows/inbox/CONTACTS.md)。
- **M0 任務書已寄出** → `wf/inbox/m0-bootstrap.md`＋`implementer-onboarding.md`，等實作 agent 接手。
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
