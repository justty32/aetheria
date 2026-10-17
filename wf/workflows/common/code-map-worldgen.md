# code-map — Worldgen

← [code-map](code-map.md)

## `core/worldgen/` — Region 十二階段生成

此目錄從 seed 與參數產生地形、人文骨架及落地資料，並提供決定論 hash 與診斷。
`core/worldgen/region_generator.h` 是門面，彙整設定、階段、骨架與診斷介面；
`region_*.h` 按這些責任分群。`gen_*.h` 共享 stage id、格網、噪聲與 hash 工具。

## 檔名如何對應生成階段

`stage_*.cpp` 是第 1～7 階段的板塊、高度、侵蝕、氣候、河流、生態與地物；
侵蝕階段是量化閘口，地物群處理放置約束，biome 分類介面隔離 terrain／relief 裁決。
`field_redistribution.*` 是高度（identity）／濕度（陸地整數 rank remap）的分布重整接縫。

人文階段以職責前綴命名：`history_*.cpp` 負責第 8 階段選址、災變與古道；
`city_*.cpp` 與 settlement 評分群負責共用純評分、canonical 分級選點及第 9 階段城市；
`road_*.cpp` 負責第 10 階段工程路徑、MST 與補環路；`portal_*.cpp` 負責第 11 階段
候選、邊界落點與補路。第 12 階段由 capital、influence、governance、faction 前綴群
負責首都、全域認領、治理釋回與勢力編排。`civ_tiles.*` 是人文共用底圖。

`region_*.cpp` 承接 seed／參數 hash、骨架建置、實體落地、階段／結果 hash 及灰階診斷。
尋找某階段時先按前綴定位，不以單一巨型實作檔作入口。

## 生成與執行期隔離

`aetheria_worldgen_objects` 可見 repo 根目錄與 `core/ai/include/`，
不含 `core/runtime/include/`，因此不能 include 跨 zone 公開介面。
`cmake/check_worldgen_cross_zone_isolation.cmake` 以 compile-fail 見證維持護欄；
不要為方便共用而加入 runtime include path。其他生成隔離與效能證據見
[測試詳圖](code-map-tests.md)。
