# code-map — Narrative 與 Script

← [code-map](code-map.md)

## `core/narrative/` — 湧現任務與事件

此目錄從 Region／Site 真值偵測需求，提供五種湧現任務、運糧／清剿歸約與事件 feed。
`emergent_quest*.cpp` 依任務編排、觀測及完成歸約分工；`narrative_event.*`
提供結構化 i18n 事件、具名參數與唯讀 feed。任務與命運模板證據歸 narrative 測試目錄。

## `core/script/` — Lua 5.4 規則擴展

`script_engine*.cpp` 依引擎、catalog、掛勾與沙箱分工，建立逐次全新 `_ENV` 的 sol2 沙箱，
提供事件／劇情／勝敗掛勾、lint、呼叫量測及原子熱重載。
`*_impl.h` 放引擎內部狀態，`*_detail.h` 共用內部 helper；
`script_world.*` 將穩定物件 id 對到 Region owner 真值並記錄事件。

公開入口 `core/script/include/aetheria/script/context.h` 只提供查詢、驗證寫入及注入 RNG；
世界真值接點留在 `core/script/context_internal.h`。
受限使用者只取得 `core/script/include/`，看不到 repo 根目錄。
`cmake/check_script_context_isolation.cmake` 會驗證受限編譯失敗、加入根目錄後編譯成功，
並清除環境 include path，避免假陽性。

六條 Lua 決定論鐵律、同 seed 世界雜湊、原子熱重載及 min-of-5 效能由 script 測試目錄驗證，
compile-fail 見證與共用量測位置見 [測試詳圖](code-map-tests.md)。
