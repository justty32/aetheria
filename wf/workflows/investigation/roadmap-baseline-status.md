# roadmap 重排前的現況基線

← [investigation](README.md)｜2026-09-09｜基準 `main` `8161ab4`

## 問題

使用者要求重新規劃 roadmap。本次先只回答三件事：

1. 舊里程碑宣稱完成的東西，哪些現在還找得到程式碼或測試程式？
2. 哪些只能證明「當時有回報過」，不能當作現在已是完整遊戲？
3. 舊 M10 停在哪裡，已知未解問題是什麼？

**證據分三種**：「舊回報」是文件或 git 當時記錄的結果；「現存證據」是本次實際看到的原始碼與測試程式；「本次執行」是這次親手跑的驗證。**本次沒有編譯、沒有跑測試，所以不宣稱目前全部綠燈。**

## 結論

舊 roadmap 把「核心機制有測試」、「從 Godot 摸得到」和「玩起來像完整遊戲」混成同一種完成。目前可信的是：三層、回合、展開收回、存讀檔、角色檔、歷史日誌與最小操作迴圈都有現存程式碼或測試程式。

但 M6/M7 的一些規則仍只是 core 零件，Lua 沒有接進可玩回合，Godot 仍是單張程式生成圖配側邊文字控制台。M9 的主線、音景、真美術與真正吃圖路徑都沒有可信的完成證據。M10 的舊計畫停在 3a，但後續順序正等這次 roadmap 重排，不能直接當成下一件事（`wf/SESSION-LOG.md:15-18`）。

## 現況

### 現在還找得到的實作證據

- 舊 M1–M5 不是純文件：Site 來回三次 hash 相同的測試程式在 `tests/runtime/playable_session_test.cpp:31-35`；存檔、推進三旬、載入後再存的測試程式在 `tests/runtime/session_persistence_test.cpp:81-100`；Site 卸載等價性另有 `tests/site/site_unload_equivalence_test.cpp`。這只證明測試存在，本次沒有執行它們。
- 最小可玩迴圈仍在：`PlayableSession` 有移動、推進、戰鬥和進出 Site/Local/Dungeon 的公開命令（`core/runtime/playable_session.h:160-214`）；bridge 有新遊戲、存讀檔、移動、推進與戰鬥綁定（`bridge/aetheria_core.cpp:278-306`）。
- M10 前半有直接程式碼：存檔、載入和日誌重放介面在 `core/runtime/playable_session.h:149-154,217-225`；`sim replay` 會分開比對 zone、raws、history head 和合成世界 hash（`sim/world_hash.cpp:194-244`）；存檔格式仍是 v23（`core/serialize/zone_codec.h:13`）。
- 可編輯沙盒仍是明確需求：進行中可加文明、勢力、角色、城鎮、兵種與劇情（`design/rules/runtime-injection.md:1-19`），世界與角色分檔、世界自帶 raws（`:40-55`），注入要是可重放的歷史（`:67-79`）。
- 原則仍有用：種類是資料（`design/principles.md:69-75`）、結構變更只在回合尾（`:93-106`）、確定性（`:108-116`）、程式碼要拆得走（`:118-128`）。

### 舊回報有說，但不該直接當成產品現況

- `wf/SESSION-LOG.md:34-54` 寫過「M0～M8 完成、414/414」；`wf/SESSION-LOG.md:98-105` 又記錄 M10 當時的 423/432 綠燈。這些是舊驗收回報，不是本次執行結果，也不證明玩家看得到全部 core 功能。
- 力量來源仍只找到 `core/rules/` 實作與測試用法；bridge/Godot 沒有對應介面。Lua 有 `ScriptEngine`（`core/script/script_engine.h:58-99`），但現在 `PlayableSession::advance_xun()` 傳入的回調沒有腳本回合（`core/runtime/playable_session.cpp:1267-1299`）。因此「M7/M8 完成」不等於這些系統在玩法流程內成立。
- Godot 目前仍在一個 view 裡用程式碼組側邊欄、按鈕和 `TextureRect`（`godot/main.gd:76-180`）；專案原始碼仍找不到 `TileMap`、`Camera2D`、音訊串流或消費 `VisualRef` 的路徑。這能操作，但不是完整的三層遊戲呈現。
- M9 真素材仍受使用者尚未提供的色盤、參考圖、prompt 與音訊規格影響（`wf/WAIT_USER.md:13-25`）；主線與音景也沒有本次可驗證的完成證據。

## 差距

1. `design/milestones.md:8-19` 只列 M0–M9，沒有已動工的 M10；同一張表也沒有區分 core 機制、可操作介面與完整體驗。
2. 魔法／信仰／種族規則和 Lua 引擎有零件，但現在的可玩回合沒有接上它們。
3. Godot 還沒有三層各自的畫面架構、鏡頭、真素材與音景；M8 只能當作最小操作路徑成立。
4. M10.3a 的交接已明記多角色槽重放 hash 對不上，被 session 拒絕的命令也沒有「未套用」日誌語意（`runtime-injection-wave2-handoff.md:62-71`）。
5. 同一交接也記錄首次 manifest 寫出前崩潰、`history.log` 尾筆截斷無法自動恢復（`:73-78`）；這些仍是已知開口。
6. mark／獨特物件與 root 成長軸仍是使用者明確擱置項（`design/milestones.md:48-55`），不能把它們當成漏掉的普通工作。

## 牽扯到的部份

- 舊里程碑與進度語意：`design/milestones.md`、`wf/SESSION-LOG.md`。
- 三層可玩門面：`core/runtime/playable_session.{h,cpp}`、`bridge/aetheria_core.{h,cpp}`、`godot/main.gd`。
- 不可只用「有測試」宣稱完成的系統：`core/rules/power_sources.*`、`core/script/*`、`tests/rules/power_sources_test.cpp`、`tests/script/lua_sandbox_test.cpp`。
- 可編輯沙盒現有地基：`core/history/*`、`core/runtime/turn_commit.*`、`core/serialize/*`、`sim/world_hash.*`、`tests/runtime/history_log_test.cpp`。
- 舊 M10 的完成範圍與未解風險：`wf/workflows/plan/runtime-injection.md:44-64,91-100`、`wf/workflows/plan/runtime-injection-wave2-handoff.md:8-15,62-84`。
- 本次沒有跑 build、CTest、Godot 或視覺／音訊驗證；要了解「現在能否全部通過」，本報告沒有提供那種證據。
