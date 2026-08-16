# 信：M0.5 Zone 實作完成回報

**寄件人**：gpt-sol 實作者
**收件人**：**Opus 5 規劃者**
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**對應任務**：[`done/m0_5-zone.md`](m0_5-zone.md)
**實作 commit**：`5e8236d`（Zone）、`39656be`（M0.1 審閱追加項）

## 實際做了什麼

- `core/zone/zone_key.h:12`：依定案位元佈局實作全 constexpr `ZoneKey`、欄位讀取、
  `parent_of`／`child_key`，以及 0 保留、永不復用的 Detached 單調配發器。
- `core/zone/zone.h`：Region／Site／Local／Detached 共用同一個可移動不可複製 `Zone`；
  納入 `entt::registry`、垂直 layers、LOD、pin、`last_saved_tick`。`TileGrid` 暫為 1×1
  `uint16_t` 實質圖層，註解明記 M1 換 SoA。
- `core/zone/zone_store.*`、`core/zone/zone_manager.*`：以 `unique_ptr<Zone>` 轉移所有權的
  `ZoneStore` 介面、in-memory 後端、完整生命週期 API、註冊順序 tick 與 FIFO 命令緩衝。
- `tests/zone/zone_test.cpp`：位元窮舉、邊界、root、Detached、生命週期三契約、缺失載入、
  決定論與 substantive unload/load 共 12 項 Zone 測試。
- `vcpkg.json`／CMake：加入 EnTT 3.16.0；`aetheria_core` 仍為純 C++ target。
- `sim/main.cpp`：建立並印出 Root → Region → Site → Local 小樹。
- 依 `done/m0-1-review.md` 補上 bridge Tick 域檢查與 GDScript `INT64_MAX` 驗證；
  `.gitignore` 改為 `build*/`。

## Done when 逐條核對

- [x] 三層 round-trip。測試實際窮舉 65,536 個 Region id、16,777,216 組 Site 座標、
  1,048,576 組 Local 座標；每一筆都驗 `parent_of(child_key(...))` 與欄位讀回。
- [x] Root／Region／Site／Local／Detached 五個已定義 level，以及 region/site/local/detached
  每欄 0、最大值都有測試；Detached 60-bit 最大值後再次配發會 fail-fast。
- [x] `parent_of(root/detached) == root`；root 永久載入、不 tick，unload／destroy 皆回 false。
- [x] Detached destroy 後再配發得到下一號，不復用舊號。
- [x] tick 中直接 `materialize`／`load`／`unload`／`destroy` 都有 death case；真實失敗輸出：

```text
structural_exit=134
AETH_CHECK failed: !in_tick_
(/home/lorkhan/repo/game_dev/aetheria/core/zone/zone_manager.cpp:78)
```

- [x] 不對外提供 `Zone*`。`get` 回 `optional<ZoneHandle>`，handle 只含 key；
  `loaded_keys`／`tick_order` 也回值。嘗試把結果存成 `Zone*` 的編譯器輸出：

```text
error: cannot convert ‘std::optional<aetheria::zone::ZoneHandle>’
to ‘aetheria::zone::Zone*’
```

- [x] `load(不存在)` 回 false，`require(不存在)` 丟 `runtime_error`，各有測試。
- [x] 同一串兩回合命令跑兩次，逐 tick 訪問序列與最後 loaded key 集合完全相同。
- [x] Debug 四 target 零警告，CTest 19/19；Release core/tests/sim 零警告，CTest 19/19；
  `CoreIsolation.CompileCommands` 通過。
- [x] sim 印出四層小樹；Godot headless editor／主場景 exit 0。
- [x] commit 到 `main`，沒有 push。

## 四個實作回饋

1. **「一切皆 zone」沒有裂開。** 四種非 root zone 的資料與生命週期完全共用；Region／Site／
   Local 只差 key.level。Root 只有「不參與一般 tick、不可逐出」的生命週期特例，Detached 只有
   定址特例，都不需要另一種 Zone struct 或 manager。
2. **命令緩衝是 `vector<variant<Materialize, Unload, Destroy>>` FIFO。** 三個 queue API 只能在
   tick 內呼叫；tick 結束先退出結構鎖，再依排隊順序逐項執行。不同 command 型別避免裸 enum
   + 無效 payload，vector 讓重播順序明確。若 system 丟例外，本回合 queue 清空後重拋，避免
   半套結構命令在未知時點延後執行。
3. **`Zone*` 以 API 形狀擋。** `ZoneHandle` 可安全跨 tick，因為只有 key；`get` 每次重新檢查
   manager 是否仍載入該 key。C++ 無法阻止惡意程式從 backend 的 owning `unique_ptr` 自行偷指標，
   但正常 system／查詢 API 沒有任何 borrowed Zone pointer/reference 可存。
4. **EnTT 決定論這輪沒有立刻咬人。** M0.5 只建立 registry，不迭代 view；zone tick 順序由
   明確的 registration-order vector 決定，loaded set 由 `std::map` 以 key 排序。之後一旦加入
   ECS system，仍必須遵守先依穩定 key 排序 entity 的既有規則。

## 自行決定的細節與現實差異

- `child_key(root, x, y)` 將 `x` 解讀為 16-bit `region_id`，要求 `y == 0`；因 root→Region
  沒有第二個座標欄位，這是唯一不丟資訊的對應。
- 為滿足「不跨 tick 持有 Zone*」，`get` 用 `nullopt` 取代表格中的 `nullptr`；缺失語意相同，
  但型別上不可能誤存 pointer。這是刻意且已由 compile-time assertion／實際編譯失敗證明。
- `unload` 把當前 tick 寫進該 zone 再移交 store；測試刻意在 Tick 100／250 逐出不同 zone，
  證明單槽活儲存允許不同 `last_saved_tick`，且 1-tile 實質資料仍存在。
- M0.1 審閱追加裁定後，bridge 先呼叫 `is_representable`；域外回空 Dictionary，core 的
  `AETH_CHECK` 仍維持 abort。Godot 實測 `INT64_MAX` 印出 `Invalid Tick rejected: true` 且 exit 0。

## 測試結果原文

```text
# Debug full build / CTest
[8/9] Linking CXX executable aetheria_tests
100% tests passed, 0 tests failed out of 19
Total Test time (real) = 1.67 sec

# Release core/tests/sim / CTest
[10/11] Linking CXX executable aetheria_tests
100% tests passed, 0 tests failed out of 19
Total Test time (real) = 0.46 sec

# sim
zone tree:
  level=0 key=0 parent=0
  level=1 key=1152939096792891392 parent=0
  level=2 key=2305860618586947584 parent=1152939096792891392
  level=3 key=3458782123193806857 parent=2305860618586947584

# Godot
Invalid Tick rejected: true
exit=0
```
