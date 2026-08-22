# code-map — `core/local`

← [code map](code-map.md)｜[conventions](conventions.md)

Local 是 L2→L3 的純 C++ 生成與展開層；Godot 不持有其中狀態。

## 公開入口與分派

| 檔案 | 職責 |
|---|---|
| `core/local/local_tiles.h` | 單一 z 層 SoA、Local 慢／快變數、路線 B 輸出 |
| `core/local/local_projection.cpp` | Site tile → Local 慢變數、local seed、共用 BoundaryProfile |
| `core/local/local_materialize.*` | 依 structure 分派路線 A/B/C，落到 `LocalPayload::layers` |
| `core/zone/zone.h` 的 `LocalPayload` | 同一 Local zone 的 `z → LocalTiles` 程序層集合 |
| `core/local/local_reduction_schema.h` | 建築段、控制點、採集點、通道四類 Local 歸約觀測 |
| `core/local/local_reduction.*` | 四列量測、Local key/LOD 驗證，以及套用到父 Site tile 的唯一 writer |

## 執行期探索

| 檔案 | 職責 |
|---|---|
| `core/local/local_navigation.*` | 跨 Local zone 格位址、規範化邊位址與可注入門狀態 |
| `core/local/local_fov.*` | 整數 DDA 逐格 FOV；只以 `peek_edge` 判斷牆／門遮蔽 |
| `core/local/local_movement.*` | 四鄰接探索步判定；區分開門、上鎖、牆與未知邊界 |

## 生成路線

| 檔案 | 職責 |
|---|---|
| `core/local/local_generation.cpp` | 路線 B：開放地表、道路／河流 crossing、散布與物件 |
| `core/local/local_buildings.h` | 路線 A 公開型別、生成、居民延遲具象化、不變式與正規化 hash |
| `core/local/local_building_generation.cpp` | A1 地基內插、層初始化與路線 A 編排 |
| `core/local/local_building_geometry.cpp` | A2～A4、A7：排屋、房間切分、門窗與垂直連結 |
| `core/local/local_building_content.cpp` | A5～A6：資料驅動家具、居民統計／進屋具象化、不變式與 hash |
| `core/local/local_building_detail.h` | 路線 A 實作檔共用的格索引、對稱邊寫入與階段入口 |
| `core/local/local_underground.h` | 路線 C 公開型別、生成、可達性／垂直一致性與正規化 hash |
| `core/local/local_underground_generation.cpp` | 地表路線 B、structure 深度與負 z 層的編排 |
| `core/local/local_underground_geometry.cpp` | 礦脈掘進、房間直折走廊，以及路線 A 拆除式遺跡 |
| `core/local/local_underground_validation.cpp` | 跨負 z 層 flood fill、不變式與決定性 hash |
| `core/local/local_underground_detail.h` | 路線 C 實作檔共用格操作與單層結果 |
| `core/local/dungeon.*` | 地城深度曲線、三種參數化房間／內容、機關互動、Boss 與光源壓力 |
| `core/local/dungeon_state.h` | Local zone 的已觸發機關與已領寶藏持久狀態 |

## 共用演算法與資料

- `core/spatial/recursive_partition.*`：Site 街廓以一格街道切分，Local 房間以格邊切分；
  同一份有界遞迴實作，兩層只換 config。
- `core/rules/ruleset_load_local_buildings.cpp`、`data/local_buildings.toml`：房屋尺度、房間下限、
  牆／門／窗、垂直層機率、居民統計與各房型家具表。
- `core/rules/ruleset_load_site_city.cpp`、`data/site_city.toml`：特殊 structure def 的地下種類與
  1～16 層深度；特殊 def 不進一般城市配額。
- `data/edges.toml`：路線 A 沿用 `EdgeDef`，門／窗仍是邊屬性。
- `core/rules/dungeon_rules.h`、`data/dungeon.toml`：三種地城參數、深度／光照／cleared
  曲線與四類 `TrapDef`；`ruleset_load_dungeon.cpp` 在載入期驗證。

## 測試

`tests/local/local_building_test.cpp` 覆蓋尺度、房間門不變式與封死反例、家具／居民實體數、
垂直層、正規化決定性 hash、10 ms 預算；Site/Local 共用切分的雙層見證分別在
`tests/site/site_skeleton_test.cpp` 與該檔。

`tests/local/local_fov_test.cpp` 覆蓋邊遮蔽負向控制、光照、門、跨 zone 退化、對稱性、
決定性與暖機後 min-of-5；`tests/local/local_movement_test.cpp` 覆蓋四鄰接、門鎖與未載入鄰區。

`tests/local/local_reduction_test.cpp` 以非空四列、空 Local 無寫入、`200→175→150`
絕對快照與語意順序無關雜湊驗證 Local→Site；共用 apply 的故障注入會與既有 Site→Region
測試同時紅。

`tests/local/local_underground_test.cpp` 覆蓋三種 structure 深度、全格／房間可達性、單走廊
封死負向控制、60～80% 遺跡拆除、垂直雙向一致、正規化 hash 與 10 ms 預算。

`tests/local/dungeon_test.cpp` 覆蓋 100 座線索相關性、三種共用深度曲線、引敵觸發機關、
Boss 破階循環、v20 Local 持久層、cleared 十次產出、光源耗盡分布、歷史來源與 min-of-5。
