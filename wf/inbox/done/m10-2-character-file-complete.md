DONE

# M10.2 角色檔完成回報

## 結果

- 新增獨立 `core/runtime/character_save.{h,cpp}`，角色檔位於
  `saves/<世界>/chars/<角色>.bin`，使用 cereal PortableBinary 與原子替換寫入。
- `kCharacterFormatVersion = 1`；`kSaveFormatVersion` 維持 23，未修改。
- bridge 的 `save_game(slot, character)`／`load_game(slot, character)` 已帶角色名，新增
  `list_characters(slot)`；`godot/main.gd` 有角色名輸入與成功／失敗訊息。

## 驗收 1：兩角色互不污染

`CharacterPersistence.TwoCharactersColdRoundTripAndWorldHashIsolation`：角色 A 移動並推進
2 旬、進 Site／Local、開門、接任務後存檔；B 從初始六欄開始，走締約後進 Local 的另一路並
存檔；冷載 A 後 `CharacterState` 六欄逐項等於 A 存檔當下，冷載 B 亦等於 B 存檔當下，且
`A != B`。兩檔列舉結果固定為 `A, B`。

## 驗收 2：往返與決定性

- A、B 都經 session 銷毀後以新的 Ruleset／FileZoneStore／PlayableSession 冷載。
- A 的六欄完整 `operator==`；同一 A 狀態在世界後續變化前後重存，角色檔 bytes 均與第一次
  寫入完全相同。
- `CharacterSave.CodecIsDeterministicAndRejectsVersionOrWorldMismatch` 另直接驗證 codec
  往返、同狀態兩寫位元相同，以及角色版本不符的清楚錯誤。

## 驗收 3：負向控制

- 刪除 `<slot>/chars/A.bin` 後，`FileZoneStore` 仍成功開世界 manifest，B 仍可冷載且六欄
  相等；補回 A 不需改任何世界檔。
- 把 A 複製到另一個 seed=999999 的世界後拒絕，實際輸出：

```text
cross_world rejected=1 error=角色檔載入失敗：.../other-world/chars/A.bin：角色檔 world_seed 與世界不符：檔內=515151 世界=999999
```

- codec 測試另以錯誤 expected `raws_hash` 驗證第二個世界身分欄位也會拒絕。

## 驗收 4：world-hash 排除角色檔

實際以指定 CLI 對同一世界的 0／1／2 角色副本執行：

```text
$ aetheria_sim verify world-hash <chars=0>
world_hash=16682231759512528432 zone_count=5 elapsed_ms=15.8609
$ aetheria_sim verify world-hash <chars=1>
world_hash=16682231759512528432 zone_count=5 elapsed_ms=15.3821
$ aetheria_sim verify world-hash <chars=2>
world_hash=16682231759512528432 zone_count=5 elapsed_ms=15.4367
```

`sim/world_hash.cpp::find_zone_files` 在遞迴器進入槽根的 `chars/` 時停止遞迴；
`FileZoneStore::stored_keys` 同步採相同排除，避免載過角色檔的世界在下次 `save_all`／鏡像時
誤解析角色名為 ZoneKey。兩處仍保留原有 canonical path 檢查。

## 驗收 5：Local 門是共享世界態

- `local::LocalDoorState` 是 Local zone registry 上的單例 component，內容是已開啟的
  canonical `std::set<LocalEdgeAddress>`；已加在 `AllComponents` 尾端。
- `door_query` 不再吃 session bool，而是讀該 component；`open_door_and_move` 寫入實際門
  edge，FOV、移動與 `local_view` 都經同一 query。
- 正規化世界雜湊包含已開門 edge；zone codec encode／decode 驗證 component 只能出現在
  Local zone 且至多一份。
- 測試中 A 開門後存檔，B 冷載可見門已開；再存、銷毀 session、冷載 A 與 B，兩者仍都
  可見門已開。

完整驗證：

```text
cmake --build build -j2
ctest --test-dir build --output-on-failure --parallel 2
100% tests passed out of 423
Total Test time (real) = 48.17 sec
```

## `kCharacterFormatVersion` v1 欄位（位元流順序）

1. `format_version` (`uint32`)
2. `world_seed` (`uint64`)
3. manifest `raws_hash` (`uint64`)
4. `player_army_id.uid` (`StableId` 的 `uint64` 值)
5. `residence_id`（`region`／`site`／`local`／`dungeon` 穩定字串，不存 enum 下標）
6. `local_z` (`int8`)
7. `local_player_x` (`uint16`)
8. `local_player_y` (`uint16`)
9. `accepted_quest_id` (`optional<uint64>`)

## 模組與依賴

- `character_save` 依賴：C++ filesystem／字串值型別、cereal PortableBinary、
  `world::StableId`；不 include `PlayableSession`、bridge 或 Godot。
- 依賴 `character_save`：`PlayableSession` 只以 `export_character_state`／
  `import_character_state` 搬六欄並呼叫檔案入口；bridge 只用列舉入口與 session API。

序列化欄位順序、版本／世界身分檢查、原子檔案 I/O 全在 `character_save.cpp`，沒有一行長進
`PlayableSession`。

## 狀態邊界與假設

- 六欄仍判定為玩家態：`player_army_id_` 只保存玩家選到哪個世界 army；army 的位置、勢力、
  power 與玩家控制旗標仍在 Region registry 世界態。匯入時若 StableId 不存在或不是玩家
  部隊，直接拒絕，不為了往返測試製造替身。
- `residence_`、`local_z_`、玩家 Local x/y 是角色視角／位置；載入時依駐留層重新 acquire
  Site／Local，Dungeon 的負 z 層不存在就拒絕。
- `accepted_quest_id_` 是玩家選擇；`quests_` 仍由世界態重算。已接受 id 若無法重建則拒絕，
  不靜默清空。
- `local_door_open_` 是唯一確認為偽裝玩家態的世界事實，已完全移除 session 成員並搬到
  Local component。未發現其餘五類狀態其實應搬進世界檔。
- 角色名採單一檔名語意：空字串、`.`、`..`、`/`、反斜線與 NUL 均拒絕；本輪不做刪除 UI。
