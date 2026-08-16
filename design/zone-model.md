# Zone 模型與生命週期

> **M0.5 的直接前置。** 三層地圖在程式裡是同一種東西：zone。
> 這份定死它的**結構與生命週期 API**；**定址另見 [zone-addressing.md](zone-addressing.md)**，
> **執行期契約（tick 內結構變更、作用域借用、命令緩衝）另見 [zone-contracts.md](zone-contracts.md)**。
> 存檔與序列化見 [zone-save.md](zone-save.md)（目錄／路徑／manifest）與
> [zone-save-format.md](zone-save-format.md)（位元流格式）；繼承來源見 [medps-inheritance.md](medps-inheritance.md)。

## 一切皆 zone

**沒有獨立的「世界地圖系統」。** Region、Site、Local 共用同一套 `Zone` 與 `ZoneManager`，
差別只在 `level` 欄位與各自掛的 system。這條繼承自 medps
（`medps/docs/work/design/zone_layers.md:36-42`）。

```cpp
struct Zone {
    ZoneKey        key;             // 身分 + 位址（zone-addressing.md）
    entt::registry reg;             // 這個 zone 的所有實體
    SpatialPayload payload;         // 空間資料，依 level 分流（見下）
    LodLevel       lod;             // 執行期，不序列化
    bool           pinned;          // 執行期，不序列化
    Tick           last_saved_tick;
};
```

- **垂直層收在同一個 zone**：地城、礦坑、下水道是 `layers` 的負 z 鍵，**不是**另一個 zone。
- **`parent` 不存**——從 `key` 算得出來（見 [zone-addressing.md](zone-addressing.md)）。
- `registry` 不可複製 → `Zone` 只能移動。
- 地圖是 zone 的固有結構，**不走 ECS**。代價是 registry snapshot 不含它，
  存檔要分兩塊處理（見 [zone-save-format.md](zone-save-format.md)）。

### 空間 payload 依 level 分流（定案）

三層的 tile 欄位集**本來就不同**（Region 有氣候／海拔／owner，Local 沒有），
但「一切皆 zone」要求 `Zone` 只有一種型別。解法是把異質性收在**一個欄位**裡：

```cpp
struct RegionPayload { std::map<int8_t, RegionTiles> layers; };   // L1
struct SitePayload   { std::map<int8_t, SiteTiles>   layers; };   // L2
struct LocalPayload  { std::map<int8_t, LocalTiles>  layers; };   // L3
using SpatialPayload =
    std::variant<std::monostate, RegionPayload, SitePayload, LocalPayload>;
```

- **變體選擇在 payload 層，不在每個 z layer。** 否則同一個 zone 的不同 z
  可能混進不同層級的 schema。
- **不變式**：`payload` 的 alternative 必須與 `key` 的 `level` 相符。
  建構時與 decode 時都要驗，不符即 fail-fast。
- Root 與 Detached 用 `monostate`——它們沒有 tile。
- 垂直層（`layers` 的 z 鍵）仍在各 payload **內**，那條沒變。

為什麼不是別的：**單一大 TileGrid 塞滿所有欄位**會讓每個 Local 都背著它永遠用不到的
氣候欄（L3 要串流大量 zone，這筆很貴）；**`Zone<Level>` 模板**會讓 `ZoneManager`
跟著裂成三份，直接違反「一切皆 zone」；**虛擬繼承**被
[cpp-conventions.md](cpp-conventions.md) 排除，也會毀掉 SoA 的快取友善性。

變體的 alternative 數量是**設計定死的三層 + 無**，不會成長——所以它不觸犯
「種類一律是資料」那條（[definitions.md](definitions.md)）：那條管的是**種類**，這是 **schema**。


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

## 成長軸不變量

規模現實：3～5 個 Region × 12288 格 ≈ 5 萬個 Site，再 × 4096 = **兩億個 Local**。
永遠不可能全載，也不可能有全域清單。

所以 [observer.md](observer.md) 那條繼承自 medps 的不變量在這裡是硬性的：
**持久／常駐結構只能隨「已載入 zone 數」或「已造訪 zone 數」成長。**
由此推出：manifest 不放 zone 清單、路徑由 key 推導、事件信箱掛在 tile 上、
`uid_index` 只在已載入 zone 上。

## 待細化

- `SiteTiles`／`LocalTiles` 的欄位集（L1 的 `RegionTiles` 已定，見 [worldmap.md](worldmap.md)；
  L2／L3 等 M3／M5 再定——現在定一定定錯）
- 各層的 system 註冊與 tick 分派（不同 stride，見 [outline.md](outline.md)）
- root 的分塊策略（若它真的長太大）
- 跨 zone 實體搬移函式（消費 `ReturnTrail`、維護 `uid_index` 的單一入口）
