# 執行期注入：已經有的地基與插入點

← [runtime-injection-status.md](runtime-injection-status.md)（結論與差距）｜[investigation/README](README.md)

**日期**：2026-08-27。這份只列**已經存在、不必重做**的東西——
下一棒規劃方案時，先確認哪些是接線問題而不是從零蓋。

## 一、存檔已經是目錄，不是單一 blob

`saves/<slot>/manifest.bin` + `root.bin` + `<桶>/<完整 16 碼 key>.bin`；
桶名取 splitmix64 混合後的 2 碼、共 256 桶（`core/zone/file_zone_store.cpp:85-109`），
寫入走 tmp + rename 原子替換、內容 zstd 壓縮（`core/zone/save_manifest_io.cpp:37-89`）。

⇒ 裁定說的「分成世界／角色兩塊只是多一個根，不是重構存檔格式」**屬實**。

## 二、「存檔自帶 raws」已經做了一半

zone 檔頭寫入 terrain／relief／feature／edge 的**字串 id 表**
（`core/serialize/zone_encode.cpp:118-122`）；載入時 `build_remap` 逐條用字串
重查當前 Ruleset、再把每格的整數下標換算過來
（`core/serialize/zone_decode.cpp:26-55, 88-95, 144-147`）。

- **新增地形定義不會弄壞舊存檔**；**刪除或改名**既有 id 才會 throw。
- 世界雜湊與外交存檔走同一條字串 id 路線
  （`core/serialize/normalized_state_hash.cpp:433-471`、`core/serialize/zone_diplomacy_codec.cpp:58,152`），
  所以雜湊不受 def 排列順序影響。

⚠ **但只有這四類有 id 表。** 存檔裡的建築是寫死的 enum
（`PersistentBuilding::type` 是 `BuildingType`，`core/site/site_projection.h:94`），
沒有走 remap；那條路上沒有這層保護。

⚠ 另一個已知的洞：remap 只檢查「字串 id 還在不在」，**不檢查 def 的內容有沒有變**。
同一個 `terrain.swamp` 把 move_cost 改掉，存檔照樣載入、沒有任何警告。

## 三、def 的字串 id 已有全域唯一性與前綴強制

`register_global_id` 要求每個 id 帶型別前綴（如 `"terrain."`），並塞進**橫跨所有型別共用**
的單一集合，重複即 `throw`（`core/rules/toml_read.h:108-126`）。
整數 id 則是「載入順序的下標」（`append_def` 用 `defs.size()`），跨版本不穩定——
所以持久狀態一律該存字串 id。18 個內容型別已經是這個形狀。

## 四、zone 樹本來就能長

- `ZoneManager::materialize` / `adopt`（`core/zone/zone_manager.h:59-60`）：
  對一個還沒有內容的 key 建出 zone。
- `DetachedZoneKeyAllocator`（`core/zone/zone_key.h:145-163`）：
  **執行期單調配發全新 zone key**，地城已經在用——
  這是目前唯一「跑起來之後真的長出新 zone」的**既有先例**。
- Region 每一格都有 `settlement` 與 `site` 欄位（`core/world/region_tiles.h:125`），
  長度＝格子數。⇒ 「執行期多一座城」**不是容量問題**，是缺 API 與插入點。

## 五、原則七的插入點：有形狀，沒接線

`ZoneManager` 的契約寫著「tick 內的結構變更只能排進 FIFO」：
`queue_materialize` / `queue_unload` / `queue_destroy`（`core/zone/zone_manager.h:106-108`）
把命令推進 `commands_`，`tick()` 跑完 system 後 `flush_commands()` 才真的執行
（`core/zone/zone_manager.cpp:189-201, 232`）。**語意正是注入需要的那種延後結算。**

⚠ 但這三個 `queue_*` 在 `core/`、`bridge/` 裡**沒有任何呼叫端**。
而回合的七階段（`core/world/region_movement.h:99-107`）末端
`TurnStage::TurnEnd` 目前只是一行 `notify`（`core/world/region_turn.cpp:181`），
是空的。`ZoneManager::tick()` 在產品程式碼裡只有 `core/site/site_materialize.cpp:355`
一個呼叫端，`PlayableSession::advance_xun()` 從沒呼叫它。

⇒ **注入要掛的那根釘子已經釘好了，只是兩端還沒接起來。**

## 六、其它可直接沿用的

- 勢力已有資料形態：`data/civilization.toml` 的 `[[faction_defs]]` 三筆（性格七維 + 字串 id）。
- 具名 NPC 已在 Site 存檔白名單內（`core/site/site_projection.h:118-129`）。
- manifest 已存生成參數雜湊並在載入時比對（`core/zone/save_manifest_io.cpp:98,145-157`）——
  「世界的身分」這個概念已經落地過一次，可作為「世界存檔要帶哪些身分資訊」的範本。
