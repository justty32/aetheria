# Zone 執行期契約

> 從 [zone-model.md](zone-model.md) 拆出。那份講 zone 的**結構與生命週期 API**，
> 這份只回答一件事：**zone 在執行期必須守住什麼**——三條契約、作用域借用、命令緩衝的形狀。
> 三條契約是一組：契約一（tick 內禁止 zone 結構性變更）靠命令緩衝落地，
> 命令緩衝也正是契約三（`Zone*` 不跨 tick 持有）的作用域借用之所以安全的原因。
> 結構與生命週期 API 見 [zone-model.md](zone-model.md)；繼承來源見 [medps-inheritance.md](medps-inheritance.md)。

## 三條執行期契約（繼承 medps）

寫進 `zone_manager.h` 的註解，並靠測試守住：

1. **tick 內禁止 zone 結構性變更。** system 呼叫 `materialize`/`load`/`unload`/`destroy`
   會讓 `zones_` 容器 rehash 或刪元素 → 迭代器 UB。
   需要在 system 內造 zone 時，走**命令緩衝**：排隊，回合尾端統一執行。
   這與 [principles.md](principles.md) 原則七是同一條。
2. **存檔目錄＝單槽活儲存。** 存檔目錄就是世界的權威狀態，
   `unload`／逐出隨時寫檔；`save_all` 是檢查點，不是槽位快照。
   各 zone 凍結於不同遊戲時刻是**接受的語意**（`last_saved_tick` 記錄它）。
   **載入不消耗磁碟檔**：zone 被載入後，記憶體是該 zone 的權威狀態，
   磁碟保留上一個檢查點。這讓 crash 最多退回上次寫檔，而不是連檔案都沒了。
3. **`Zone*` 不跨 tick 持有。** 逐出會析構 `Zone`。長駐的東西一律存 `ZoneHandle`（只含 key），
   每次要用再查。

## 存取：作用域借用（scoped borrow）

⚠ **這一節是踩到坑之後補的。** 契約三原本只說「不准跨 tick 持有 `Zone*`」，
沒說「系統要怎麼讀寫 zone 資料」——結果第一版實作用「**完全不交出任何存取**」
滿足了它：`ZoneManager` 只回 `ZoneHandle`，`registry` 與 `layers` 從外面碰不到。
每條驗收都過，但那套 API 什麼玩法都寫不了。

**規則不是「永不交出 `Zone&`」，是「`Zone&` 不得活過這次借用」。**

```cpp
template <typename F> decltype(auto) with(ZoneHandle, F&& f);   // f 收到 Zone&
```

- 存取一律經 **callback**，引用只在 callback 內有效，**不得逃逸**（不存成員、不回傳）。
- `tick()` 傳給 system 的也是借用（`Zone&`），不是光禿禿的 handle。
- 借用期間**不可能**發生逐出——結構性變更全部排進命令緩衝、回合尾端才執行（契約一）。
  這正是契約一與契約三互相支撐的地方：**有了命令緩衝，借用才安全**。
- 借用不是萬無一失（惡意程式仍可把引用抄走），但它讓**正確寫法是最省事的寫法**，
  這就夠了。契約靠測試守，不靠型別系統證明。

## 命令緩衝的形狀（定案）

`std::vector<std::variant<Materialize, Unload, Destroy>>`，**FIFO**。

- 三個 `queue_*` 只能在 tick 內呼叫；tick 跑完先退出結構鎖，再依排隊順序逐項執行。
- 每種命令一個型別，**不用裸 enum + 無效 payload**。
- **system 丟例外時，本回合 queue 清空後重拋。** 半套的結構命令延到未知時點執行，
  比直接失敗糟得多。
