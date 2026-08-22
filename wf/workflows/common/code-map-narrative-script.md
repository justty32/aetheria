# core/narrative 與 core/script 導航

← [code map](code-map.md)｜[conventions](conventions.md)

## `core/narrative`

`emergent_quest.*` 從 Region／Site 真值偵測需求與運糧／清剿歸約；
`narrative_event.*` 提供結構化 i18n 事件、具名參數與唯讀 feed。

## `core/script` — Lua 5.4 規則擴展層

`script_engine.*` 建立逐次新 `_ENV` 的 sol2 沙箱、事件／劇情／勝敗掛勾、lint、呼叫量測與
原子熱重載；`script_world.*` 將穩定物件 id 映射到 Region owner 真值並記錄事件。

公開 `include/aetheria/script/context.h` 只有查詢、驗證寫入與注入 RNG；世界真值接點留在
受限 target 看不到的 `context_internal.h`。

## 測試

- `tests/narrative/`：五種湧現任務、運糧／清剿歸約、命運模板與事件 feed。
- `tests/script/`：六條 Lua 決定論鐵律、同 seed 世界雜湊、原子熱重載與 min-of-5 效能。
- `tests/compile_fail/script_context_world_truth.cpp`：Context 世界真值 include 隔離的雙向證據。
