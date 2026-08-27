DONE

# M10.1 完成回報 — 世界檔自帶 raws

## 驗收證據

1. **新槽完整複製與 manifest 雜湊**：`SaveRaws.OldWorldIsolatedFromGlobalDataAndTamperingRejected` 使用每測試私有 data，逐一斷言 `slot/raws` 恰有 22 個 TOML、檔名集合及每檔 binary bytes 都與來源相等，並斷言 `manifest.raws_hash == save_raws_hash(slot)`。複製先寫 `raws.tmp`，全部成功及算完 hash 後才 rename；目的 `raws/` 已存在即拒絕，之後不改寫。

2. **改全域 data 不影響舊槽**：同一測試在舊槽建立後，只修改私有的「全域 data」，每次均從舊槽自己的 raws 冷開；前後輸出如下。

   ```text
   global_change=add_terrain before_hash=12616416018089442560 after_hash=12616416018089442560 before_terrain_count=7 after_terrain_count=7
   global_change=swamp_move_cost before_hash=12616416018089442560 after_hash=12616416018089442560 before_move_cost=3 after_move_cost=3
   global_change=faction_count before_hash=12616416018089442560 after_hash=12616416018089442560 before_factions=3 after_factions=3
   ```

3. **槽內 raws 單 byte 竄改會拒絕，還原後可開**：在 `terrain.toml` 尾端附加一個空白 byte；輸出如下。

   ```text
   raws_tampered rejected=1 error=世界狀態雜湊無法開啟 /tmp/aetheria-ruleset-16987400536424-0/old-world/manifest.bin：raws 內容雜湊不符：檔內=9358848766047242766 當場重算=16219517487113457523
   raws_restored accepted=1 world_hash=12616416018089442560
   ```

   同測試另刪除複製槽的整個 `raws/`，確認開槽 fail-fast，錯誤明列「存檔 raws」目錄不存在或不可讀。

4. **修改後的新槽吃新 data，並與舊槽並存**：私有 data 新增可用地形（連同必要的 site projection）、把 swamp `move_cost` 改成 9、把 `faction_count` 及 defs 改成 4；新遊戲外交初始化因此改為依 ruleset faction count 建立，不再寫死 3。

   ```text
   coexisting_worlds old_hash=12616416018089442560 old_terrain_count=7 old_move_cost=3 old_factions=3 new_hash=17405052134408193673 new_terrain_count=8 new_move_cost=9 new_factions=4
   ```

5. **CLI 與全套測試**：`verify world-hash` 在載入任何 `--data-dir` 前分流，函式簽名已移除外部 Ruleset；舊槽測試刻意傳入不存在的 `--data-dir`，仍成功證明未讀取它。

   ```text
   aetheria_sim 舊槽 verify：world_hash=8485915008936685094 zone_count=4 elapsed_ms=5.59478
   aetheria_sim 新槽 verify：world_hash=8485915008936685094 zone_count=4 elapsed_ms=5.66085
   100% tests passed out of 421
   Total Test time (real) = 48.33 sec
   ```

   `cmake --build build --parallel 2` 成功；`ctest --test-dir build --output-on-failure --parallel 2` 為 421/421，測試總數未減。

## v23 格式變更

- `core/serialize/zone_codec.h`：`kSaveFormatVersion` 22 → 23。
- `core/zone/save_manifest.h`：在既有、獨立的 `generation_parameters` 後新增 `uint64_t raws_hash`；兩種 hash 沒有合併或取代。
- `core/zone/save_manifest_io.cpp`：manifest binary encode/decode 在 `generation_parameters.groups` 與 `now` 之間新增 `raws_hash`。
- `tests/zone/zone_codec_test.cpp`、`tests/site/site_building_mapping_test.cpp`、`tests/site/site_observation_persistence_test.cpp`：三處版本 static_assert 同步為 23。
- `tests/zone/file_zone_store_manifest_test.cpp`：手寫 manifest fixture 增加 raws hash 欄位，預期固定大小由 149 改為 157 bytes；其餘受影響的錯誤訊息期望同步為 v23。

## 第 5、6 件的最終做法

- **save 導向 CLI**：`sim/main.cpp` 在全域 Ruleset 載入前先處理 `verify world-hash`；`sim/world_hash.{h,cpp}` 的 public API 只收 slot path，自行檢查 `slot/raws`、載入 Ruleset、核對 manifest raws hash。理由是從型別與控制流一起消除「外部傳錯 Ruleset」的入口。`gen` 仍吃 `--data-dir`；一般 `--save-dir` 若是新槽先複製基準 data，既有槽則只讀自己的 raws。
- **exported build 基準 raws**：bridge `new_game` 改為要求 Godot 傳入絕對 data path；CMake build 後把 22 個來源 TOML stage 到 `godot/data/`，`godot/export_presets.cfg` 將它們收入 PCK。因 C++ `std::filesystem` 不能讀 PCK 內的 `res://`，`godot/main.gd` 逐 byte materialize 到絕對 `user://packaged-data` 後傳給 bridge。`AETHERIA_DEFAULT_DATA_DIR` 僅留在非存檔的 bridge `generate_region` 開發入口，以及 sim 的新世界／gen 預設；任何既有存檔路徑都不拿它當事實來源。

## 模組與依賴（原則九）

- `core/runtime/save_raws.{h,cpp}` → 只依賴 C++ 標準函式庫／filesystem；不 include PlayableSession、bridge、Godot 或 Ruleset。
- `PlayableSession`、`FileZoneStore`、sim world-hash／新槽流程、bridge 存讀檔 → 依賴 `save_raws` 提供路徑、原 byte 複製、排序內容 hash 與核對。

## 假設與路徑稽核

- raws 定義為來源目錄頂層全部 `*.toml`；目前正式基準恰為 22 個，不遞迴、不帶其他副檔名。
- 完整的使用者世界槽必須有非零 v23 `raws_hash`；PlayableSession load 與 world-hash 對缺欄位／缺 raws 都 fail-fast。`FileZoneStore` 仍允許低階單元測試建立 `raws_hash == 0` 的非完整 store fixture，但不把它視為可載入的正式世界槽。
- exported 基準檔以目前固定的 22 個檔名 materialize；每次開新遊戲可刷新 `user://packaged-data`，已建立世界的 `slot/raws` 不受影響。
- 驗證機只有 Godot 4.7.2，而專案指定 4.7.1；本輪完整 C++／bridge build 使用 godot-cpp 內附 4.7 API 設定完成，未把本機 4.7.2 冒充 4.7.1，也未另跑 Godot 實機驗收。
- 稽核 formal save 路徑後，未發現 static／singleton、def 指標或整數下標仍從全域 data 取得 Ruleset：PlayableSession、bridge load、sim world-hash 都由該 slot raws 建立。`tests/support/ruleset_fixture.h` singleton 語意未改，僅限純讀測試；raws 新測試全部使用私有 tmp 複本。
