# M5.13 完成回報 — ZoneManager rematerialize 單一入口

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

**回報任務**：[`done/m5-13-rematerialize-callback.md`](done/m5-13-rematerialize-callback.md)

## 落地內容

- `ZoneManager::acquire` 成為「取得完整可用 zone」的單一入口：快照存在時先由
  store 解碼持久層再交 callback 重展開；不存在時 callback 收到空 pointer 並直接生成。
- callback 可在 manager 建構時設定（給 `CrossZoneRuntime`），也可每次 `acquire`
  傳入（給需當下 Region 慢／快變數的 Site streaming）。只有 callback 成功回傳且
  Region／Site／Local 程序層完整時才放入 manager。
- `load` 與 `require` 保留原本「只解持久層」的獨立語意；`materialize`
  舊入口也未併入 `acquire`。
- Site 新增擁有已解碼 `Zone` 的 `rematerialize_site_zone` overload，實際走「慢變數
  重建骨架 → 當下快變數填充 → 疊回持久層與 digest 補算」；沒有把 `load`
  與 `rematerialize` 合併。
- `SiteStreamingCoordinator::ensure_loaded` 與 `CrossZoneRuntime::migrate_entity`
  都已改走 `acquire`，不再各自用 `contains/load` 拼取得邏輯。

## 驗收實測

| 判準 | 結果與數字 |
|---|---|
| 程序層真的回來 | Local 冷 `load` 為 **0 tiles**，`acquire` 後為 **4096 tiles**；Site 骨架亦為 **0 → 4096** |
| 持久層沒被蓋掉 | Site 存檔有 **1** 個 Idle 持久建築，重展開後仍為 **1** 個、座標仍為 **(13,25)**；Local 持久 registry 實體也是 **1 → 1** |
| 兩個入口還在 | `ZoneManager::load` 單獨呼叫成功，Local 程序 tiles 實測仍為 **0**，持久實體為 **1** |
| 無存檔直接生成 | 空 store 上 `acquire` 回 handle，產出 **4096 tiles** |
| `migrate_entity` 解鎖 | 目的 Local 只在 store 時搬移回 `true`；目的地重展開後 **4096 tiles**，持久 uid 7001 仍在，搬入 uid 7002 可解引 |
| 失敗不給空殼 | callback 故意回空 Local `Zone(key)`：`acquire.has_value=0`、`manager.get.has_value=0`；既無存檔也無 callback 時搬移回 `false`，目的 zone 仍不存在 |
| 決定性 | 同一串冷載入與 A→B→A 搬移連跑兩次，兩 zone 正規化雜湊皆同：`8766003291610299300, 9226398417218568126` |

## 負向控制（真的紅）

將 `ZoneManager::acquire` 故意改成只做 `load(key)`、不呼叫 materializer，直接執行：

```text
./build/aetheria_tests \
  --gtest_filter=ZoneManagerAcquire.ColdLoadRestores4096TilesAndPreservesPersistentObject
```

測試 exit **1**，唯一失敗為：

```text
Expected equality of these values:
  tile_count
    Which is: 0
  aetheria::local::kLocalTileCount
    Which is: 4096
zone_acquire raw_tiles=0 rematerialized_tiles=0 persistent_objects=1
```

持久實體當時是 **1** 個，所以不是空 digest 或 codec 失敗；紅燈精確抓到「程序層
仍為 0」。恢復 callback 後同測試變為 **0 → 4096** 並通過。

## 被迫加的機制

1. callback 必須「擁有」解碼快照，否則 manager 若先接管空程序層，重展開途中
   失敗就會留下可被 `get` 看見的空殼。現在是完成且驗證後才單點接管。
2. manager 必須有分層完整性閘門：Region 層的 layout、Site procedural layout、
   Local z=0 與各層 layout 任一不完整都不回 handle。
3. Site 舊 rematerialize 入口自己呼叫 manager `load`，無法交給新的擁有式 callback；
   因此補了「接收已解碼 Zone」overload，三步邏輯仍共用原 `prepare_site` /
   `restore_loaded_site`。
4. 首次完整 CTest 揭露 `require()` 被誤改為 acquire 會破壞 CLI 兩程序存檔。
   最終恢復 `require/load` raw 語意，只有明確需要完整 zone 的呼叫者走 `acquire`。

## 現有測試仍不能證明

- Local 目前沒有專屬持久位置 component；因此「原座標」用正式 Site
  `PersistentBuilding::tile` 驗證，Local 只驗持久 registry 實體集合不漏失。
- `normalized_state_hash` 依設計不納入可重算的 Local tiles；決定性雜湊能證持久
  與搬移狀態不漂移，tiles 的決定性仍由現有 Local 生成測試與本輪 4096 非空閨門分別把關。
- callback 只能使用呼叫者提供的當下 parent／慢快變數；測試能證 Site streaming
  與跨 zone 搬移已接通，不能防止未來新呼叫者誤用 raw `load/materialize`。
- 未可重現注入 allocator 失敗；store 解碼例外在 `migrate_entity` 邊界會轉為
  `false`，但本輪沒有為所有 I/O 錯誤類型做窮舉故障注入。

## 完整驗證

- `cmake --build build --parallel 2`：全 target 建置成功、零警告。
- `ctest --test-dir build --output-on-failure`：**244/244 綠**，78.14 s。
- `./build/aetheria_sim --tick 62208000`：exit 0，固定 zone tree/hash 輸出正常。
- Godot 4.7.1 headless editor：全新專案首掃 exit 139（已知情形），原樣重跑 exit 0；
  主場景 exit 0。

未新增 `.cpp`；未修改 `design/`、`core/local/local_fov.*`、`core/local/local_movement.*`；
未 push。
