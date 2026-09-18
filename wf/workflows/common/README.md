# common — 跨工作流共享（入口）

← [INDEX](../../INDEX.md)｜[AGENTS.md](../../../AGENTS.md)

不專屬任一工作流、各工作流共用的東西。這是本區的**入口**。

<!-- wf-nav -->
| 路徑 | 內容 | 來源 |
|------|------|------|
| [gotchas.md](gotchas.md) | 共通踩坑（第二次撞到才記）；會咬人的專案陷阱另在 [KNOWN-TRAPS](../../KNOWN-TRAPS.md) | kernel |
| [user.md](user.md) | 使用者偏好、確認邊界、分支慣例 | kernel |
| [reply-style.md](reply-style.md) | 對話回覆風格：先講結論、一步一動作、何時可以破例 | kernel |
| [conventions.md](conventions.md) | **程式碼慣例**（架構鐵律：核心 C++ 不依賴 godot-cpp、Godot 端不持有玩法狀態；`AllComponents` 登記）| dev 包 |
| [code-map.md](code-map.md) | **原始碼導航**：目錄職責、檔名慣例、各領域分頁（world／worldgen／site／local／narrative-script）；拆檔／重構後要回來更新 | dev 包 |
| [data-files.md](data-files.md) | 資料檔契約 `wf-table/1`：給 AI 消化的條列 >1 KB 抽 `.json`／`.csv`、`tabledb.py` 讀寫；給人導航的連結表留 md | kernel |
| [data-files-fmt.md](data-files-fmt.md) | json 值裡跨層路徑用 `$fmt` 代號展開，省 `../../../` | kernel |
<!-- wf-insert:COMMON -->

> 過時文檔封存進 `common/archive/`（見 [STRUCTURE](../../STRUCTURE.md)）。
