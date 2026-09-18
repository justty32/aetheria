# runtime — 執行期編排

純 C++ 的跨 zone 操作、可玩情境與存檔／歷史重放。玩法真值仍由既有世界、
Site、Local 與 zone 模組提供；本目錄串接它們。

## PlayableSession 入口

- [playable_session.h](playable_session.h)：既有彙整入口，保留三個 enum 的定義路徑，
  以及使用這些列舉子的事件／戰鬥報告／覆蓋摘要型別。
- [playable_session_views.h](playable_session_views.h)：部隊、格網與回合檢視型別。
- [playable_session_class.h](playable_session_class.h)：原樣搬移的類別宣告；呼叫端仍
  include 原入口。直接 include 此檔時，也會先取得入口的型別定義。
- [playable_session_detail.h](playable_session_detail.h)：跨實作檔共用的格索引、
  地城入口、門狀態查詢與駐留名稱 helper 宣告；單檔 helper 留在匿名 namespace。

## PlayableSession 實作

以下檔名均以 `playable_session` 開頭。

| 檔案 | 職責 |
|---|---|
| `.cpp` | 建構、新世界 UID 設定、載入入口與恢復交易 |
| `_scenario.cpp` | Region、部隊與覆蓋 Site 初始情境 |
| `_materialize.cpp` | Site／Local 具現化與持久態重展開 |
| `_load.cpp` | 載入後 zone 綁定、部隊 handle 與情境重建 |
| `_site.cpp` | Site／Local 取得與 Site 進退 |
| `_city.cpp` | 親自與代管城建 |
| `_quests.cpp` | 任務更新、接取與清剿 |
| `_local.cpp` | Local 進退、門狀態與開門移動 |
| `_dungeon.cpp` | 地城進退、下層與清理 |
| `_diplomacy.cpp` | 外交初始化與和平條約 |
| `_turn.cpp` | 移動命令、遭遇偵測與回合推進 |
| `_battle.cpp` | Region／Site 戰鬥、命運與世界回寫 |
| `_armies.cpp` | 部隊、Region 格態與時鐘查詢 |
| `_coverage.cpp` | 覆蓋情境的批次摘要 |
| `_site_view.cpp` | Site 格網快照 |
| `_local_view.cpp` | Local 視野與格網快照 |
| `_history.cpp` | UI 事件、命令記錄與歷史重放入口 |
| `_character.cpp` | 角色駐留狀態匯出／還原 |
| `_save.cpp` | 世界槽 raws 檢查與存檔交易 |
| `_calibration.cpp` | Site 往返雜湊與城建校準 |

## 其他入口

| 檔案 | 職責 |
|---|---|
| `include/aetheria/runtime/cross_zone.h` | 跨 zone 公開介面，保留 include 可見性護欄 |
| `cross_zone.cpp`、`cross_zone_migrate.cpp` | 格／邊／實體查詢；可 rollback 的實體搬移 |
| `cross_zone_detail.h` | 跨檔共用的合法座標與地表層查詢 |
| `turn_commit.h`、`turn_commit.cpp` | journal-first 記錄、世界提交與 marker I/O |
| `turn_commit_replay.cpp` | 角色命令上下文還原與命令重放 |
| `turn_commit_payload.cpp`、`turn_commit_detail.h` | 重放 payload 純文字欄位解碼 |
| `session_persistence.*` | 世界存讀編排 |
| `character_save.*` | 獨立角色 codec 與列舉 |
| `save_raws.*` | 存檔基底 TOML 的複製、雜湊與路徑 |
