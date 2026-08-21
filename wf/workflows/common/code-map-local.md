# code-map — `core/local`

← [code map](code-map.md)｜[conventions](conventions.md)

Local 是 L2→L3 的純 C++ 生成與展開層；Godot 不持有其中狀態。

## 公開入口與分派

| 檔案 | 職責 |
|---|---|
| `core/local/local_tiles.h` | 單一 z 層 SoA、Local 慢／快變數、路線 B 輸出 |
| `core/local/local_projection.cpp` | Site tile → Local 慢變數、local seed、共用 BoundaryProfile |
| `core/local/local_materialize.*` | 依 structure 分派路線 A/B，落到 `LocalPayload::layers` |
| `core/zone/zone.h` 的 `LocalPayload` | 同一 Local zone 的 `z → LocalTiles` 程序層集合 |

## 生成路線

| 檔案 | 職責 |
|---|---|
| `core/local/local_generation.cpp` | 路線 B：開放地表、道路／河流 crossing、散布與物件 |
| `core/local/local_buildings.h` | 路線 A 公開型別、生成、居民延遲具象化、不變式與正規化 hash |
| `core/local/local_building_generation.cpp` | A1 地基內插、層初始化與路線 A 編排 |
| `core/local/local_building_geometry.cpp` | A2～A4、A7：排屋、房間切分、門窗與垂直連結 |
| `core/local/local_building_content.cpp` | A5～A6：資料驅動家具、居民統計／進屋具象化、不變式與 hash |
| `core/local/local_building_detail.h` | 路線 A 實作檔共用的格索引、對稱邊寫入與階段入口 |

## 共用演算法與資料

- `core/spatial/recursive_partition.*`：Site 街廓以一格街道切分，Local 房間以格邊切分；
  同一份有界遞迴實作，兩層只換 config。
- `core/rules/ruleset_load_local_buildings.cpp`、`data/local_buildings.toml`：房屋尺度、房間下限、
  牆／門／窗、垂直層機率、居民統計與各房型家具表。
- `data/edges.toml`：路線 A 沿用 `EdgeDef`，門／窗仍是邊屬性。

## 測試

`tests/local/local_building_test.cpp` 覆蓋尺度、房間門不變式與封死反例、家具／居民實體數、
垂直層、正規化決定性 hash、10 ms 預算；Site/Local 共用切分的雙層見證分別在
`tests/site/site_skeleton_test.cpp` 與該檔。
