# code-map — Runtime 與 History

← [code-map](code-map.md)

## `core/runtime/` — 執行期編排

此目錄串接 Region、Site、Local、zone 與規則，負責可玩情境、駐留進退、命令提交與存讀。
細部入口見 [runtime README](../../../core/runtime/README.md)。

`core/runtime/playable_session.h` 是呼叫端門面，保留三個 enum 及事件、戰鬥報告、覆蓋摘要；
`playable_session_views.h` 收部隊／格網／回合檢視型別，`playable_session_class.h` 收類別宣告。
這是同一個 PlayableSession 的介面分工，Godot 與 bridge 只取得批次快照。

`playable_session*.cpp` 依職責實作同一類別：無後綴者負責建構與載入入口；
`playable_session_<職責>.cpp` 的情境、具現化與載入群建立世界及重綁 handle，
Site／Local／地城／城建／任務／外交群負責互動，回合與戰鬥群協調世界回寫，
檢視群產生快照，歷史／角色／存檔／校準群處理持久化接點與等價量測。
`*_detail.h` 只共享格索引、門查詢、入口等內部 helper。

## 跨 zone 與交易

`cross_zone*.cpp` 負責格／邊／實體查詢與可 rollback 的搬移。
公開護欄入口在 `core/runtime/include/aetheria/runtime/cross_zone.h`；只有取得
`core/runtime/include/` include path 的 target 才能以公開 include 名稱使用它。
worldgen 的 include path 不含此目錄，不能把執行期跨 zone 依賴帶入生成層。

`turn_commit*.cpp` 依提交、payload 解碼、重放分工：先寫 journal，再提交世界與 marker；
恢復時還原角色命令上下文。`session_persistence.*` 編排世界存讀，`character_save.*`
是獨立角色 codec／列舉，`save_raws.*` 管世界槽基底 TOML 的不可變複製、內容雜湊與路徑。

## `core/history/` — 追加式歷史

本目錄保存不解讀玩法 payload 的 append-only 歷史、雜湊鏈驗證與重放介面。
`history_log*.cpp` 分離記錄／驗證和磁碟 I/O，`*_detail.h` 供內部跨檔共用；
命令語意與世界恢復仍由 runtime 解讀。

驗證導航見 [測試詳圖](code-map-tests.md)：runtime 涵蓋 session、交易、角色檔與 raws；
跨 zone 查詢、搬移和 rollback 歸 zone 測試目錄。
