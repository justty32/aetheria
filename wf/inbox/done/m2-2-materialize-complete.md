# 信：M2.2 完成 — Site zone 展開與真實持久建築

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**回覆哪封**：`m2_2-materialize-and-persistent.md`

## 完成內容

- `populate(SiteSkeleton, const SiteFastVars&)` 已落地；聚落等級改變程序分區密度，
  `build_site_skeleton` 的簽章仍只接受 `SiteSlowVars`。編譯期測試同時釘住
  `populate` 吃得到 fast、拿 `SiteSlowVars` 呼叫不了，以及 skeleton 拿不到 fast。
- `materialize_site_zone` 由 `(region_id, x, y)` 推導 Region／Site `ZoneKey`，建立
  `SitePayload` 三層資料並停在 `L_COARSE`。
- 聚落初次展開會在可建地放一棟 `SettlementHall`。它有 `SiteXY`、`BuildingType`，以及
  `Active / Idle / Derelict / Ruined` 四態；本輪沒有建築系統、城建循環或 L_FULL 推進。
- `SavedSiteLayers` 仍只有 `SitePersistentLayer`，codec 直接從這份 type list 展開欄位；
  程序骨架／分區與易失層不進存檔。持久建築存載往返保持座標、型別、狀態。
- 世界級正規化 hash 以 canonical 順序納入持久建築；程序層與 runtime LOD 不納入。

## 關鍵證據

兩個獨立存檔槽各自展開一次，再實際執行 `aetheria_sim verify world-hash`：

```text
first:  world_hash=6118786596596188375 zone_count=4 elapsed_ms=0.457755
second: world_hash=6118786596596188375 zone_count=4 elapsed_ms=0.466361
```

精準測試用最小兩-zone 世界再做一次展開兩次與建築狀態負向控制：

```text
site_materialize_first world_hash=12681280711799693346
site_materialize_second world_hash=12681280711799693346
site_building_state_changed world_hash=18068079511531492422
```

座標穩定性（改 owner、settlement、ever_realized 後重新投影）：

```text
site_coordinate_stability x=2 y=0
original_skeleton=12067252577927825256
reprojected_skeleton=12067252577927825256
buildable=1
```

`buildable=1` 同時要求 ground 不是水、該格四邊沒有 river flag，因此不是只比 skeleton hash。

效能只量本輪既有 Debug build，沒有另建 Release：

```text
site_materialize_Debug_ms=0.469727
```

低於 30 ms 預算；不把 Debug 數字冒充 Release。

## 存檔格式變更

`kSaveFormatVersion` **8 → 9**。v9 的 Site payload tag（2）後新增由 `SavedSiteLayers`
展開的 `SitePersistentLayer`：目前是 building vector，單筆含 `x:uint16`、`y:uint16`、
`type:uint8 enum`、`state:uint8 enum`（由 cereal PortableBinary 編碼）。manifest 欄位排列沒變，
但共用的 `format_version` 值同步升為 9。

這不能在 v8 下靜默加入：v8 的 Site tag 後面直接是 registry archive；若沿用版本，v9 decoder
會把 registry 開頭誤讀成持久層 vector，造成位元流錯位。現行政策又是版本不符大聲拒讀、沒有遷移，
所以必須升版。

`design/zone-save-format.md` **需要跟著改**：第一塊目前只描述 Region layers，應補上 Site tag 2
後只寫 `SavedSiteLayers`，並明列 Procedural／Volatile 不寫盤；版本數字若文件有列也應改為 9。

## 三個問題

1. **改建築狀態時世界級雜湊真的變了嗎？**
   有。`12681280711799693346 → 18068079511531492422`。這條負向控制直接改
   `Active → Idle` 後重存、重新走磁碟 world hash；持久層確實進了 hash，不是空通過。
2. **存檔格式動了嗎？動了什麼？**
   有，v8 → v9；Site payload 新增上述持久層位元流。`zone-save-format.md` 需要更新，理由與精確欄位見上節。
3. **`ZoneKey` 由座標推導，有沒有哪裡逼我想存一個 id？**
   沒有。唯一可能形成壓力的位置是「Site payload 載回時如何知道它屬於哪一格」，但既有 zone
   檔頭的 `Zone::key` 已完整攜帶 `(region_id,x,y)`，codec 直接用它；payload 與建築都沒有新增
   `site_id`。建築本輪也不需要物件 id，只有座標／型別／狀態。

## 完整驗證

- `cmake --build build --parallel 2`：四 target 成功、零警告。
- `ctest --test-dir build --output-on-failure`：**145/145** 通過。
- `CoreIsolation.CompileCommands`：通過，`aetheria_core` 仍零 godot-cpp。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- Godot headless editor 與主場景：兩者 exit 0。
