# M5.1 完成回報——串流協調器與全局時鐘裁定

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

**對應任務**：[m5-1-streaming-coordinator.md](m5-1-streaming-coordinator.md)

## 結果

| 驗收項 | 實測 |
|---|---|
| 時鐘唯一 | `rg hours_into_xun core tests` 為 0 筆；`CityEconomy` 不再序列化或 hash 該欄。先推進 Site A 36 小時，再把 A、B 同批推進 204 小時，Region 全局時鐘到 1 旬、Region 結算 1 次。中途加入的 B 不再持有／同步私人時鐘。 |
| 存檔 v14 | `kSaveFormatVersion == 14`；把已寫出的版本欄改成明確的 v13，`FileZoneStore.RejectsZoneFormatVersionMismatch` 大聲拒讀。 |
| 3×3／5×5 | 中心場實測 `L_FULL=9`、`L_COARSE` 外圈 `=16`，首次載入 25。跨格時只重算 desired field；結構變更留到 `finish_turn()`。 |
| 踱步不抖動 | 邊界往返 10／20 次：有緩衝時皆為 loads `30/30`、unloads `0/0`，不隨 N 線性增長。 |
| 批次真的走批次 | 9 個 `L_FULL` 一次交給批次：`batch_sites=9`、`site_xun_boundaries=9`、`region_xun_advances=1`，全局時鐘恰為 1 旬。 |
| 回合尾端 | 跨格後、`finish_turn()` 前 LOD／load／unload 全不變；尾端才凍結 5 個。模擬 1 旬期間仍 frozen=5、unload=0；下一個尾端才 unload=5。 |
| Frozen 契約 | 凍結後只留單一 `SiteDigest`，清空持久／程序／易失 live layer 與荒野程序實體；荒野 thaw 實測能重新生成實體。 |

全局 `TurnClock` 現由 Region 單一持有。Site 小時流水線逐整點推進它；到旬界時呼叫只結算、不重複推鐘的 Region 入口。既有單 Site 入口仍是一元素批次 wrapper。

## 負向控制

兩次都先改實作、以同一正向測試確認真的紅，再完整還原；不是保留在測試內的假分支。

1. **拿掉一旬延遲**：暫把 `kStreamingAbsentDelay` 改為 0。
   `SiteStreaming.BoundaryPacingLoadAndUnloadCountsStayBounded` 失敗；10 次往返變成
   loads=75、unloads=50，20 次變成 loads=125、unloads=100。還原後為
   loads=30/30、unloads=0/0。
2. **把批次退化成逐 Site 呼叫**：暫把協調器的一次批次改成 9 次單 Site wrapper。
   `SiteStreaming.PlayerFieldHasNineFullAndSixteenCoarseAndBatchesOneXun` 失敗；
   `region_xun_advances` 從 1 變 9，全局時鐘也從 1 旬變 9 旬。還原後通過。

## 抽象補強

沒有加入新的玩法機制；使用既有場強參數與四級 LOD。為把既有抽象接通，補了三個基礎接縫：

- `ZoneManager` 原本不能接管已由純函式生成的 Site，也不能在單一作用域借出多個 zone；新增 `adopt` 與不讓指標逃逸 callback 的 `with_many`。
- `RegionTurnPipeline` 原本把「推進一旬」與「結算一旬」綁死；全局時鐘由 Site 逐小時推進後，需拆出 `settle_elapsed_xun`，避免旬界重複推鐘。
- 原生命週期實作直接從 Full／Coarse 寫盤到 Absent，沒有可供一旬緩衝的記憶體 Frozen；補上 freeze／thaw／evict 三段，名稱與狀態均沿用既有 LOD。

## 驗證

- `cmake --build build --parallel 2`：成功。
- `ctest --test-dir build --output-on-failure`：217/217 通過，含 `CoreIsolation.CompileCommands`。
- `./build/aetheria_sim --tick 62208000`：成功。
- `godot-mono --headless --path godot --editor --quit-after 3`：exit 0。
- `godot-mono --headless --path godot --quit-after 5`：exit 0。
- `core/local/local_generation.cpp` 未修改；`design/` 未修改；未 push。

## 現有測試證不了的事／待規劃者處理

- v13 拒讀測試是把當前檔案版本 header 改寫為 13，能證明版本門禁，不能取代真實歷史 v13 存檔語料庫的整檔測試。
- 本輪只證明同步生成與記憶體／InMemory store 的協調路徑；背景預生成執行緒、預算逐出、`peek_*`、跨 zone 移動均依任務「不要做」未實作也未驗證。
- 任務要求維護 v14 版本沿革表，但該表位於 `design/`；使用者硬性禁止修改 `design/`，因此本 commit 只升程式版本與測試。請規劃者另行補記沿革。
