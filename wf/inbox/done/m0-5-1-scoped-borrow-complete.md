# 信：M0.5.1 作用域借用完成回報

**寄件人**：gpt-sol 實作者
**收件人**：**Opus 5 規劃者**
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**對應審查**：[`done/m0-5-review.md`](m0-5-review.md)
**實作 commit**：`b1c824f`

## 做了什麼

- `ZoneManager::with(handle, callback)` 新增 mutable／const 兩種作用域借用；handle 未載入時回
  `false`，載入時 callback 分別收到 `Zone&`／`const Zone&`。
- callback 刻意限制為回傳 `void`，正式 API 不提供把引用或指標當回傳值逃逸的路徑；需要讀值時
  由 callback 寫入呼叫端擁有的值。這不能阻止惡意程式自行保存引用，但讓正常用法維持作用域內。
- `tick()` 的 system 參數由 `ZoneHandle` 改為 `Zone&`；每次依既有 tick order 重新查出 zone，
  callback 結束後才在回合尾端套用 FIFO 結構命令。
- 完全移除 `InMemoryZoneStore::tile_count()` 測試後門；卸載／重載的實質資料改由正式 `with`
  API 寫入與讀回。
- 新增兩項驗收測試：tick 借用寫入後回合外可讀；tick 內排隊 unload 後仍能繼續使用當次借用，
  回合尾卸載並重載後資料為 callback 最後寫入的值。

## 驗收結果

- [x] Debug 四個 target 建置成功、零警告。
- [x] Debug CTest 21/21；新增兩項 Zone 測試均通過。
- [x] Release core／tests／sim 建置成功、零警告；CTest 21/21。
- [x] `CoreIsolation.CompileCommands` 通過，core 仍無 godot-cpp。
- [x] sim 小樹輸出正常；Godot headless editor 與主場景皆 exit 0。
- [x] commit 到 `main`，沒有 push。

## 驗證輸出摘錄

```text
# Debug CTest
100% tests passed, 0 tests failed out of 21
Total Test time (real) = 1.66 sec

# Release build / CTest
[10/11] Linking CXX executable aetheria_tests
100% tests passed, 0 tests failed out of 21
Total Test time (real) = 0.46 sec

# 新增測試
ZoneManager.TickBorrowWritesZoneDataVisibleAfterTheTurn ... Passed
ZoneManager.QueuedUnloadKeepsTickBorrowAliveUntilCallbackReturns ... Passed

# Godot 主場景
Aetheria core version: 0.0.1-m0
Invalid Tick rejected: true
exit=0
```
