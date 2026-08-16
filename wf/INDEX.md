# INDEX — aetheria 專案地圖

整個專案的頂層導航。aetheria = **中世紀奇幻、日式表現風格的三層嵌套地圖回合制策略遊戲；Godot 4 只做顯示／美術／音效／UI，全部玩法邏輯以 C++ GDExtension 實作。** AGENTS.md 只放主工作流 + 指向本檔；細節從這裡分流出去。

---

## Repo 佈局

**非侵入式佈局**：repo 頂層只留 `AGENTS.md` / `CLAUDE.md` 兩個 agent 入口，這套工作流 kernel 的其餘部分全部收在 `wf/`（本檔就在裡面）。理由與慣例見 `~/repo/workflows/non-invasive-import.md`。

```text
aetheria/
  AGENTS.md CLAUDE.md README.md
  CMakeLists.txt vcpkg.json
  cmake/            各 target 的來源清單、Godot 工具鏈與 CTest 腳本
  core/ bridge/ tests/ sim/   純 C++ 核心、綁定、測試、headless CLI
  godot/            Godot 4.7 顯示／整合驗證專案
  third_party/      固定版本的 godot-cpp submodule
  design/    遊戲設計文件（索引：design/README.md，由主 agent 維護）
  wf/        工作流 kernel（本檔所在）
  .claude/   commands（如 /wf-tick）
```

> M0 可編譯骨架已建立；建置與驗證見 [design/build.md](../design/build.md) 與
> [testing](workflows/testing.md)。目前尚無玩法邏輯。

### 主體

| 路徑 | 內容 |
|------|------|
| `design/` | 遊戲設計文件，索引見 [design/README.md](../design/README.md)（不歸本工作流 kernel 管，由主 agent 同步維護）。|
| `.claude/` | [commands](../.claude/commands/)（slash 指令，如 `/wf-tick`）。|
| `core/`、`bridge/`、`godot/` | 純 C++ 核心、GDExtension 綁定與 Godot 顯示層。|
| `tests/`、`sim/` | GoogleTest 與不需 Godot 的 headless CLI。|
| `cmake/` | 建置分檔：`targets_*.cmake`（各 target 的來源清單）、`godot_toolchain.cmake`、CTest 腳本。|
| `third_party/` | 固定版本的 godot-cpp submodule。|

### `wf/` — 工作流 kernel（本檔所在）

| 路徑 | 內容 |
|------|------|
| `workflows/` | 各工作流的入口與 durable 知識（派發表見 [WORKFLOWS.md](WORKFLOWS.md)）。|
| `inbox/` | agent 之間的**信件**收件匣（放信處，保持乾淨；使用方式見 [workflows/inbox/](workflows/inbox/README.md)）。|
| `WORKFLOWS.md` `INDEX.md` `DEV-GUIDE.md` `SESSION-LOG.md` `WAIT_USER.md` | kernel 的五份頂層文件 |

> 某目錄內部複雜就在該目錄放它自己的 README / INDEX，本檔只留一句話 + 連結——永遠只描述「頂層」。

## 工作流

工作流的**選擇與入口**見 **[WORKFLOWS.md](WORKFLOWS.md)** 的派發表。每個工作流的 durable 知識歸在 `workflows/<該工作流>/` 或單檔 `workflows/<該工作流>.md`（含 `archive/` 封存過時文檔），具體流程在各自入口檔。

[DEV-GUIDE](DEV-GUIDE.md) 是**被動的結構整理參考**（結構整理原則 + 四級成長軌跡）——**只在要重構/整理結構時取用**。always-on 的**鐵律**在 [AGENTS.md](../AGENTS.md)；碰原始碼的**程式碼慣例**在 [workflows/common/conventions.md](workflows/common/conventions.md)，**原始碼導航**在 [workflows/common/code-map.md](workflows/common/code-map.md)。

## 通用（跨工作流共享）

| 路徑 | 內容 |
|------|------|
| [common/README](workflows/common/README.md) | 跨工作流共通：[gotchas](workflows/common/gotchas.md) 踩坑 + [conventions](workflows/common/conventions.md) 程式碼慣例（架構鐵律：核心 C++ 不依賴 godot-cpp、Godot 端不持有玩法狀態）+ [code-map](workflows/common/code-map.md) 原始碼導航|

## 活狀態（只列還沒完成的）

三軸：進度＝我手上的、待使用者＝卡在人、信件＝agent 之間收發（像 email）。

| 檔案 | 用途 |
|------|------|
| [SESSION-LOG](SESSION-LOG.md) | 進度 hub（repo 根）→ 各工作流 session-log（open-only）|
| [WAIT_USER](WAIT_USER.md) | 待**使用者**親自做/驗證的入口（open-only）|
| `inbox/`（放信處）+ [workflows/inbox/](workflows/inbox/README.md)（使用方式）| agent 之間的**信件**（像 email，狀態靠位置：inbox 頂層＝未處理、`done/`＝已處理）|
