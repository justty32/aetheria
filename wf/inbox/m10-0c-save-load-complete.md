DONE

# M10.0c 完成回報

## 驗收證據

1. `SessionPersistence.ColdFileStoreLoadPreservesPlayableWorldSnapshotAndHash` 建立新遊戲、下達玩家移動並推進 3 旬，存檔後銷毀 session，再以新 `FileZoneStore` 冷讀。測試逐項比對 Region 地形／地貌／特徵／氣候／海拔／邊、領主／聚落／人口／秩序、部隊 faction／power／位置／控制權、外交 persistent state 與時間，全部相等。
2. 同一測試在第一次存檔後與冷讀再存後分別呼叫 `sim::world_state_hash`，兩個雜湊相等；讀檔未重跑 worldgen、建軍或外交初始化。
3. 全套 CTest 的 `SimWorldHash.Command` 以 CLI 建槽後實際執行 `aetheria_sim verify world-hash <slot>`，通過。
4. `SessionPersistence.RejectsSaveWhileEncounterIsPending` 建立 pending 遭遇後確認存檔拋錯；訊息同時含「遭遇尚未處理」與「回合尾端」，目的槽未產生 manifest。
5. `cmake --build build -- -j2` 完成；`ctest --test-dir build --output-on-failure --parallel 2` 為 419/419 通過。另跑 `aetheria_sim --tick 62208000`、Godot 4.7.2 headless editor 匯入與主場景，均以 0 結束。

## 實作摘要

- `PlayableSession` 改由建構子借用 `ZoneStore&`；新遊戲與冷讀使用不同入口。唯一 `ZoneManager` 擁有既有 root、Region、戰鬥 Site 與覆蓋 Site；session 僅保留 key／借用指標／entity handle。
- 外交狀態掛在 manager 的既有 root，未建立第二個 root。冷讀只接受 root 已有外交態的存檔。
- `ArmyState { faction, power, player_controlled }` 成為 registry 權威資料；`PlayableArmy` 只留 StableId 與 entity handle，冷讀由 registry 重建並重算任務。
- `session_persistence` 先令 manager `save_all()`，必要時把 active store 的 zone 快照同步至目的 store、移除目的端 stale zone，最後才寫 manifest。冷讀先檢查 manifest 並辨識唯一 Region。
- bridge 提供絕對路徑版 `save_game`／`load_game`／`list_saves`；讀檔失敗保留原 session。Godot 只管理 `user://saves` 槽名與 UI 訊息，不持有玩法狀態。

## v22 格式變更（無 v21 遷移，舊檔照既有政策 fail-fast）

- `core/serialize/zone_codec.h`：`kSaveFormatVersion` 21 → 22。
- `core/world/army_state.h`：新增可序列化 `ArmyState` component。
- `core/serialize/all_components.h`：在既有清單尾端註冊 `ArmyState`，不改舊 component 順序。
- `core/serialize/normalized_state_hash.cpp`：把 ArmyState 的存在、faction、power、player_controlled 納入正規化世界雜湊。
- `tests/zone/zone_codec_test.cpp`、`tests/zone/cross_zone_test.cpp`、`tests/zone/diplomacy_save_test.cpp`、`tests/site/site_building_mapping_test.cpp`、`tests/site/site_observation_persistence_test.cpp`、`tests/local/dungeon_test.cpp`：更新版本斷言／fixture 標示，並在 codec round-trip 覆蓋 ArmyState。
- `core/zone/save_manifest.h`：只把既有 manifest 型別從 `file_zone_store.h` 抽出，供抽象 store 共用；位元佈局未改。

## 模組與依賴（原則九）

- `session_persistence` → `time/tick.h`、`zone_key.h`；實作另依賴 `save_manifest.h`、`zone_manager.h`、`zone_store.h` 與 STL，不依賴 bridge／Godot。
- 依賴 `session_persistence` → `PlayableSession`（存檔與讀檔檢查）；`aetheria_core` CMake 編入它，bridge 僅透過 `PlayableSession` 間接使用。

## 被迫採用的假設

- bridge 的 `slot_path` 是單一槽目錄的絕對路徑；Godot 驗證槽名後以 `ProjectSettings.globalize_path("user://saves")` 組出路徑。
- 新遊戲在選定槽前以 `InMemoryZoneStore` 運行；首次／另存時完整同步到 `FileZoneStore`。從磁碟讀入後則直接以該 `FileZoneStore` 為 active store。
- M10 可玩存檔恰有一個 Region；零個或多個均視為損壞／不相容存檔。
- 目前固定場景的戰鬥與覆蓋座標足以由 Region key 推導子 zone key，不擴張 manifest 格式。
- 公開指令是同步的，`encounter_tile_` 是目前唯一能暴露的非回合尾端狀態；有 pending 遭遇一律拒存。
- 冷讀若缺 Region、兩個必要 Site、root 外交態或可玩所需部隊，直接報錯，不以初始化補造。

## 目前無法由 zone／manifest 重建的狀態

- 任務書允許留到 M10.2 的玩家態：residence、observer 的 x／y／z、accepted quest；玩家 army id 不直接存 session 欄位，但本輪可由唯一 `ArmyState.player_controlled` 部隊重新辨識。
- Local 的 `local_door_open_` 目前只在 session 記憶體，冷讀後回到關閉；這是尚未落 zone 的世界互動態。
- event feed、最後戰報、`revision_`、`next_event_id_` 只在 session 記憶體，冷讀後重置。尤其 `revision_`／`next_event_id_` 目前亦參與後續戰鬥的亂數輸入，因此立即世界快照一致，但讀檔後再戰鬥的未來亂數軌跡尚不能保證與不中斷遊戲一致。
- UI／量測暫存（最近城市／治安／發展 reduction 前後值、地城密度前後值、round-trip hash、校準累計）未存；湧現任務則依 persistent Site 於冷讀時重算，沒有用初始化值冒充。
