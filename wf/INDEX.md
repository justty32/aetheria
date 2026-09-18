# INDEX — aetheria 專案地圖

整個專案的頂層導航。aetheria = **中世紀奇幻、日式表現風格的三層嵌套地圖回合制策略遊戲；Godot 4 只做顯示／美術／音效／UI，全部玩法邏輯以 C++ GDExtension 實作。** AGENTS.md 只放主工作流 + 指向本檔；細節從這裡分流出去。

---

## Repo 佈局

**非侵入式佈局**：repo 頂層只留 `AGENTS.md` / `CLAUDE.md` 兩個 agent 入口與 `.claude/commands/` 適配層，這套工作流 kernel 的其餘部分全部收在 `wf/`（本檔就在裡面）。理由與慣例見 `~/repo/workflows/docs/non-invasive-import.md`；kernel 版本戳在 `AGENTS.md` 尾端。

```text
aetheria/
  AGENTS.md CLAUDE.md README.md
  CMakeLists.txt vcpkg.json
  cmake/            各 target 的來源清單、Godot 工具鏈與 CTest 腳本
  core/ bridge/ tests/ sim/   純 C++ 核心、綁定、測試、headless CLI
  godot/            Godot 4.7 顯示／整合驗證專案
  tools/art/        美術管線（確定性後處理與入庫閘）
  third_party/      固定版本的 godot-cpp submodule
  design/    遊戲設計文件（入口 design/README.md，清單 design/INDEX.md）
  wf/        工作流 kernel（本檔所在）
  .claude/   commands（/wf-lint、/wf-tick）
```

### 主體

| 路徑 | 內容 |
|------|------|
| `design/` | 遊戲設計文件。入口 [design/README.md](../design/README.md)，完整清單 [design/INDEX.md](../design/INDEX.md)（不歸本工作流 kernel 管，由主 agent 同步維護）。|
| `.claude/commands/` | slash 指令適配層：[`/wf-lint`](../.claude/commands/wf-lint.md)（文檔檢查）、[`/wf-tick`](../.claude/commands/wf-tick.md)（定期心跳）。Claude Code 只讀專案根的這層，非侵入式佈局也留在根。|
| `core/`、`bridge/`、`godot/` | 純 C++ 核心、GDExtension 綁定與 Godot 顯示層。|
| `tests/`、`sim/` | GoogleTest 與不需 Godot 的 headless CLI。|
| `cmake/` | 建置分檔：`targets_*.cmake`（各 target 的來源清單）、`godot_toolchain.cmake`、CTest 腳本。|
| `tools/art/` | 美術管線腳本（Python；不是 kernel 的 `wf/tools/`）。|
| `third_party/` | 固定版本的 godot-cpp submodule。|

### `wf/` — 工作流 kernel（本檔所在）

| 路徑 | 內容 |
|------|------|
| `workflows/` | 各工作流的入口與 durable 知識（派發表見 [WORKFLOWS.md](WORKFLOWS.md)；共享區 [workflows/common/](workflows/common/README.md)）。|
| `inbox/` | agent 之間的**信件**收件匣：頂層＝未處理、`done/`＝已處理（使用方式見 [workflows/inbox/](workflows/inbox/README.md)）。|
| `tools/` | kernel 工具：`wf-lint.sh`（檢查連結／大小／殘留）、`tabledb.py`（資料檔 CRUD／連結）、`find_big_lists.py`、`fix_moved_links.py`；資料檔契約見 [workflows/common/data-files.md](workflows/common/data-files.md)。|

> 某目錄內部複雜就在該目錄放它自己的 README / INDEX，本檔只留一句話 + 連結——永遠只描述「頂層」。

## 頂層文件

| 檔案 | 角色 |
|------|------|
| [WORKFLOWS.md](WORKFLOWS.md) | 派發器：意圖 → 工作流入口 |
| [STRUCTURE.md](STRUCTURE.md) | 結構整理參考（被動）：分層、膨脹即拆、四級成長、archive、工作流形式 |
| [SESSION-LOG.md](SESSION-LOG.md) | 進度 hub（open-only；規劃者／實作者分區塊）|
| [WAIT_USER.md](WAIT_USER.md) | 待**使用者**親自做／驗證／裁定的事（open-only）|
| [KNOWN-TRAPS.md](KNOWN-TRAPS.md) | **會在未來咬人**的專案陷阱：假通過、原則五被侵蝕、AI 在地圖上什麼都不做（`design/` 有連過來，先留在此層）|

## 工作流

工作流的**選擇與入口**見 **[WORKFLOWS.md](WORKFLOWS.md)** 的派發表。每個工作流的 durable 知識歸在 `workflows/<該工作流>/` 或單檔 `workflows/<該工作流>.md`（含 `archive/` 封存過時文檔），具體流程在各自入口檔。always-on 的**鐵律**在 [AGENTS.md](../AGENTS.md)。

| 路徑 | 內容 |
|------|------|
| [common/README](workflows/common/README.md) | 跨工作流共通：[gotchas](workflows/common/gotchas.md)、[conventions](workflows/common/conventions.md) 程式碼慣例、[code-map](workflows/common/code-map.md) 原始碼導航、[user](workflows/common/user.md) 偏好、[reply-style](workflows/common/reply-style.md)、[data-files](workflows/common/data-files.md) 資料檔契約 |
| [dispatch/](workflows/dispatch/README.md) | 派工與驗收：[OPS-NOTES](workflows/dispatch/OPS-NOTES.md) 操作心得、[OPS-VERIFY](workflows/dispatch/OPS-VERIFY.md) 驗收心得、[CODEX-PROTOCOL](workflows/dispatch/CODEX-PROTOCOL.md) 與 codex 的即時通道 |
| [team-model](workflows/team-model.md) | 誰來做：角色三層、分級表；本專案的 [TEAM](workflows/team-model/TEAM.md) 團隊制與波節奏 |
| [investigation/](workflows/investigation/README.md)、[plan/](workflows/plan/README.md)、[spec](workflows/spec.md) | 現況報告、動工計畫、規則規格（使用者的規劃文件）|
