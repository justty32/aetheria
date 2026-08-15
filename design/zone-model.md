# Zone 模型與定址

> **M0 的直接前置。** 三層地圖在程式裡是同一種東西：zone。
> 這份定死它的結構、定址與生命週期契約。
> 存檔與序列化見 [zone-save.md](zone-save.md)；繼承來源見 [medps-relation.md](medps-relation.md)。

## 一切皆 zone

**沒有獨立的「世界地圖系統」。** Region、Site、Local 共用同一套 `Zone` 與 `ZoneManager`，
差別只在 `level` 欄位與各自掛的 system。這條繼承自 medps
（`medps/docs/work/design/zone_layers.md:36-42`）。

```cpp
struct Zone {
    ZoneKey                   key;        // 身分 + 位址（見下）
    entt::registry            reg;        // 這個 zone 的所有實體
    std::map<int8_t, TileGrid> layers;    // 鍵即 z：地面 0、地下為負、天空為正
    LodLevel                  lod;        // 執行期，不序列化
    bool                      pinned;     // 執行期，不序列化
    Tick                      last_saved_tick;
};
```

- **垂直層收在同一個 zone**：地城、礦坑、下水道是 `layers` 的負 z 鍵，**不是**另一個 zone。
- **`parent` 不存**——從 `key` 算得出來（見下節）。這是座標定址的紅利之一。
- `registry` 不可複製 → `Zone` 只能移動。
- 地圖是 zone 的固有結構，**不走 ECS**。代價是 registry snapshot 不含它，
  存檔要分兩塊處理（見 [zone-save.md](zone-save.md)）。

## 定址：混合方案（定案）

> **空間 zone 用座標推導，非空間 zone 用單調序號。**
> 這是 [medps-relation.md](medps-relation.md) 繼承清單第 11 項的定案。

### 位元佈局

```
bits 63-60 : level (4)     0=Root, 1=Region, 2=Site, 3=Local, 15=Detached
level != Detached:
  bits 59-44 : region_id  (16)   哪個大陸
  bits 43-32 : site_x     (12)   在 Region 中的格位
  bits 31-20 : site_y     (12)
  bits 19-10 : local_x    (10)   在 Site 中的格位
  bits  9-0  : local_y    (10)
level == Detached:
  bits 59-0  : 單調序號
```

未使用的層級欄位一律填 0。`ZoneKey(0)` 即 root。

```cpp
constexpr ZoneKey parent_of(ZoneKey k) noexcept;   // 純位元運算，不查表
constexpr ZoneKey child_key(ZoneKey parent, uint32_t x, uint32_t y) noexcept;
constexpr uint32_t local_x_of(ZoneKey k) noexcept; // child 在 parent 的哪一格
```

### 為什麼選座標推導（而 medps 選了零語意序號）

medps 在 2026-07-22 放棄了座標語意，改用零語意 `uint64_t` 序號 + `parent` 鏈，
理由是「ToME 短名定址無座標語意也活得很好」。aetheria 走回座標推導，因為：

| 收益 | 說明 |
|---|---|
| **ChildLink 整題免費消失** | 「child 在 parent 地圖的哪一格」直接從 key 讀出。medps 把這題 defer 了（`zone-addressing-lifecycle-design.md` §3.4），aetheria 不必付這筆帳 |
| **投影天然可定址** | [interface-world-mid.md](interface-world-mid.md) 的 `project(region_tile)` 需要「這格的 Site 是誰」——一次位元運算 |
| **接邊的規範化 id 直接可算** | [edge-consistency.md](edge-consistency.md) 的 `canonical_edge_id` 需要兩側的格位，從 key 就有 |
| **不必配發、不必同步** | 沒有 `next_id` 計數器，就沒有「配發後 crash 導致 manifest 與磁碟不一致」的窗口 |
| **`parent` 不必存** | 少一個欄位、少一處可能不一致的來源 |

aetheria 的巢狀是**嚴格且稠密**的（每個 tile 恰好對應一個下層 zone），
這正是座標定址最划算的形狀。medps 的顧慮成立於它沒有落地 ChildLink 的前提下。

### 代價與對策

| 代價 | 對策 |
|---|---|
| **世界尺寸被烘進 key** | Region 與 Site 的尺寸寫進存檔的 manifest，並宣告 **IMMUTABLE**。中途更動會讓所有既有 key 失效——這與 medps 的 `WorldConfig` 是同一個取捨 |
| 欄位寬度定死上限 | 刻意給了餘裕：Region 最大 4096×4096（實際 128×96）、Site 最大 1024×1024（實際 64×64）。上限不會在可預見的未來咬人 |
| 非空間的 zone 無處安放 | 這正是 `Detached` 存在的理由 |

### Detached：非空間 zone

有些 zone 不對應世界上的任何一格：劇情用的口袋空間、傳送門後的異界、夢境、試煉場。
它們用單調序號定址，配發規則同 medps 的 `create_child`（`next_id_++`、0 保留、**永不復用**）。

`parent_of(detached)` 回傳 root。它們的「在哪裡」由玩法層自己記（通常是一個 Portal 實體）。

### Key 是位址，不是存在證明

**任何合法座標都有一個 key**，但只有被具現化過的 zone 才有磁碟檔案。
這正好對應 [interface-lifecycle.md](interface-lifecycle.md) 的 `L_ABSENT`：
位址永遠算得出來，內容按需生成或載入。
存在性檢查 = **磁碟上有沒有那個檔案**，不維護任何清單。

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
| `get(key)` | 取已載入的 zone；未載入回 `nullptr`。**不影響 LOD 分數** |
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
3. **`Zone*` 不跨 tick 持有。** 逐出會析構 `Zone`。第一個想長駐快取 `Zone*` 的
   系統出現時，強制改成 handle 式存取（存 key、每次 `get`）——不等它咬人。

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
- 命令緩衝的形狀
- root 的分塊策略（若它真的長太大）
- 跨 zone 實體搬移函式（消費 `ReturnTrail`、維護 `uid_index` 的單一入口）
