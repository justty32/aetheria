# Zone 模型與生命週期

> **M0.5 的直接前置。** 三層地圖在程式裡是同一種東西：zone。
> 這份定死它的**結構與生命週期契約**；**定址另見 [zone-addressing.md](zone-addressing.md)**。
> 存檔與序列化見 [zone-save.md](zone-save.md)（目錄／路徑／manifest）與
> [zone-save-format.md](zone-save-format.md)（位元流格式）；繼承來源見 [medps-inheritance.md](medps-inheritance.md)。

## 一切皆 zone

**沒有獨立的「世界地圖系統」。** Region、Site、Local 共用同一套 `Zone` 與 `ZoneManager`，
差別只在 `level` 欄位與各自掛的 system。這條繼承自 medps
（`medps/docs/work/design/zone_layers.md:36-42`）。

```cpp
struct Zone {
    ZoneKey                   key;        // 身分 + 位址（zone-addressing.md）
    entt::registry            reg;        // 這個 zone 的所有實體
    std::map<int8_t, TileGrid> layers;    // 鍵即 z：地面 0、地下為負、天空為正
    LodLevel                  lod;        // 執行期，不序列化
    bool                      pinned;     // 執行期，不序列化
    Tick                      last_saved_tick;
};
```

- **垂直層收在同一個 zone**：地城、礦坑、下水道是 `layers` 的負 z 鍵，**不是**另一個 zone。
- **`parent` 不存**——從 `key` 算得出來（見 [zone-addressing.md](zone-addressing.md)）。
- `registry` 不可複製 → `Zone` 只能移動。
- 地圖是 zone 的固有結構，**不走 ECS**。代價是 registry snapshot 不含它，
  存檔要分兩塊處理（見 [zone-save-format.md](zone-save-format.md)）。

## root zone

`ZoneKey(0)`，**永久載入，永不逐出，不參與一般 tick**。放跨 zone 的全局實體：

| 住在 root 的東西 |
|---|
| 勢力、外交關係、科技進度 |
| 世界圖（WorldGraph）與 Region 清單 |
| 全局時鐘、曆法、季節 |
| 劇情旗標 |
| **觀察點樹**（見 [observer.md](observer.md)） |
| **獨特物件登記表**（見 [unique-objects.md](unique-objects.md)） |

⚠ **root 的成長軸是已知風險。** root 每次開檔都必讀，它變大就直接拖慢載入。
medps 把這題列為懸置；aetheria 的觀察點樹與獨特物件登記表都住這裡，**這題更緊**——
兩者都必須有明確的數量上限與淘汰策略，見對應文件的「待解」。

## 生命週期 API

| 操作 | 語意 |
|---|---|
| `get(key)` | 查已載入的 zone，回 `optional<ZoneHandle>`（**不是 `Zone*`**）。**不影響 LOD 分數** |
| `require(key)` | 取或載入；檔案缺失＝結構損毀 → throw |
| `load(key)` | 探測性載入；不存在回 `false`，不是錯誤 |
| `materialize(key)` | 不存在就生成（走 [gen-pipeline.md](gen-pipeline.md)） |
| `unload(key)` | 寫檔後從記憶體移除。root 不可 |
| `destroy(key)` | 移除並**同步刪除磁碟檔案** |
| `tick()` | 對每個已載入 zone 依註冊順序跑 system；root 不參與 |

`require` 與 `load` 分流是刻意的：**結構引用指向的檔案缺失是損毀，該 fail-fast**；
探測性查詢則該安靜回報。medps 把這題 defer 了。

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

## 成長軸不變量

規模現實：3～5 個 Region × 12288 格 ≈ 5 萬個 Site，再 × 4096 = **兩億個 Local**。
永遠不可能全載，也不可能有全域清單。

所以 [observer.md](observer.md) 那條繼承自 medps 的不變量在這裡是硬性的：
**持久／常駐結構只能隨「已載入 zone 數」或「已造訪 zone 數」成長。**
由此推出：manifest 不放 zone 清單、路徑由 key 推導、事件信箱掛在 tile 上、
`uid_index` 只在已載入 zone 上。

## 待細化

- `TileGrid` 的實際型別（各層欄位不同，見 [worldmap.md](worldmap.md) 與 [midmap.md](midmap.md) 的 SoA）
- 各層的 system 註冊與 tick 分派（不同 stride，見 [outline.md](outline.md)）
- root 的分塊策略（若它真的長太大）
- 跨 zone 實體搬移函式（消費 `ReturnTrail`、維護 `uid_index` 的單一入口）
