# M5.9 完成回報 — Red Blob 地形管線對照

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者

## 結論

四頁不是同一配方：noise／2022 Voronoi 教學以 noise 直接給 elevation、moisture；2010
polygon generator 則先定海岸，再由距海岸／淡水定高度／濕度，且明說火山島結果並非處處適用
（[Polygonal Map Generation「4 Elevation」](http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/#elevation)）。

**本輪排序第一：先讓 M5.7 的單一 domain-warp 變因完成量測，不疊 noisy edges。** 問題是
noise 作用位置，不是有沒有 fBm：目前 fBm 只加在既成 raster Voronoi 板塊的高度上
（`core/worldgen/stage_height.cpp:82-92`）。若 M5.7 失敗，才試 noisy-edge 光柵化。

## 對照表

| 他的作法（網址／章節） | 我們現在的作法（證據） | 結構差異、值不值得改 | 成本／決定性／接邊 |
|---|---|---|---|
| 低頻定寬廣地貌、高頻定小細節；octave 是各頻率加權和（[Noise Functions「5 Frequency」「7 Combining frequencies」](https://www.redblobgames.com/articles/noise/introduction.html#frequency)；[Terrain from Noise「Elevation / Octaves」](https://www.redblobgames.com/maps/terrain-from-noise/#octaves)） | plate base + boundary effect + fBm（`core/worldgen/stage_height.cpp:82-92`）；fBm 波長／振幅逐 octave 減半（`core/worldgen/gen_noise.h:60-69`） | 已有 octave，但只當高度加數，未擾動海岸幾何。**改作用位置，不先加 octave。** | 改 plate 取樣動階段 1 與全部後段；固定 seed、座標、tie-break 可決定論。L1 無跨 Region 接邊。 |
| `e^exponent` 重塑分布；terrace 量化高度層（[Terrain from Noise「Redistribution」「Terraces」](https://www.redblobgames.com/maps/terrain-from-noise/#redistribution)） | 無重分布；海平面命中固定陸地數，再修連通並夾到海平面兩側（`core/worldgen/stage_height.cpp:95-124`） | Redistribution 可校準谷／峰，**不能修海岸**：單調 `pow` 不改百分位 land mask。terrace 加重硬階，**不做**。 | 動階段 2 及後段；避用跨平台 `pow`，用整數 LUT／rank remap。 |
| island falloff 以中心到邊界距離壓低外圈（[Terrain from Noise「Islands」](https://www.redblobgames.com/maps/terrain-from-noise/#islands)）；Voronoi 版是 noise 減邊界距離（[Voronoi Tutorial「3 Island shape」](https://www.redblobgames.com/x/2022-voronoi-maps-tutorial/#island-shape)） | 無 falloff；取高度百分位，再從最大連通塊長到目標數（`core/worldgen/stage_height.cpp:16-63,95-124`） | 保證外圈海洋，卻把 Region 推成中心島、壓過板塊意圖。**近期不做**。 | 階段 2 與後段重算；座標函式可決定論，無 L1 接邊收益。 |
| 先定海岸，elevation = 距海岸，再按 CDF 重排，保證下坡抵海（[Polygonal Map Generation「4 Elevation」](http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/#elevation)） | 先由板塊、邊界效應、fBm 得高度才切海岸（`core/worldgen/stage_height.cpp:82-118`）；priority-flood 補出口（`core/worldgen/stage_rivers.cpp:42-98`） | 因果相反。**第二順位固定 land mask 實驗**：coast distance 當低頻基底，保留板塊 ridge；不全換成單峰火山島。 | 階段 2、侵蝕、氣候、河流、biome 都變；整數多源 BFS 可決定論。接邊見下。 |
| moisture = 距淡水，再 rank-remap（[Polygonal Map Generation「6 Moisture」](http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/#moisture)） | 風掃描：海上補水、抬升耗水、比例衰減（`core/worldgen/stage_climate.cpp:87-132`）；河後只加四鄰 moisture（`core/worldgen/stage_rivers.cpp:100-127`） | 我們不是獨立濕度 noise。**不取代風／雨影**；可第三順位將一格 river bonus 改成距淡水衰減的土壤濕度。 | 動階段 5 與 biome／地物；不回寫氣候／流量以免循環。整數 BFS 決定論。 |
| 簡化版用 jittered seeds、Delaunay/Voronoi adjacency、cell noise+falloff 與獨立 moisture noise（[Voronoi Tutorial「1 Seed points」至「4 Biomes」](https://www.redblobgames.com/x/2022-voronoi-maps-tutorial/)） | 隨機 8–16 seeds；每 raster 格取最近者，四鄰 owner 不同即邊界，BFS 擴散 effect（`core/worldgen/stage_plates.cpp:55-89,92-130`） | 我們沒有 Delaunay／corner mesh。**不為形式改 mesh**；只作 noisy-edge 退路。 | 建 mesh、edge graph、光柵化與 tie-break，階段 1 及後段全變，成本高於 warp。 |
| 兩 polygon center + 兩 Voronoi corner 成四邊形，midpoint-displacement 細分共享邊（[Polygonal Map Generation「8 Noisy Edges」](http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/#noisy-edges)；[Noisy Edges「Noisy edges」](https://www.redblobgames.com/maps/noisy-edges/)） | 僅有最近 plate owner 與四鄰差異，無 corner／dual／共享線段（`core/worldgen/stage_plates.cpp:74-103`） | **不能直接套**；要影響 128×96 land mask，須建 mesh 並光柵化回 owner／boundary distance。 | stage 1 替代成本高；canonical edge id seed + 固定點細分可決定論。只用於 L2 顯示則不動 L1 狀態。 |

## 兩項特別裁決

### 1. Noisy edges 與 M5.7

**同尺度是替代，不要疊；跨尺度才互補。** warp 可直接改最近種子的取樣座標；noisy edges
須先補 dual mesh 與光柵化，故**目前不是更好的板塊方案**。先依
`wf/inbox/m5-7-coastline-noise.md:26-56` 用重合率、碎形度、三 seed 圖裁決 warp；若長毛、拓撲
破碎或仍直，再用受四邊形約束的 noisy edges **取代**。未來 L2 放大海岸時，它可與 L1 warp
分工細尺度輪廓。

### 2. 距海岸高度與接邊一致性

**L1 Region 相容；每個 Site／Local 各算則衝突。** L1 整張生成，Region 間明定不幾何接邊
（`design/worldgen-terrain.md:120-127`），故全 Region BFS 不破壞 Site 接邊；量化高度仍可作下層輸入。

降維鏈要求角→邊先裁決、面只能讀（`design/edge-consistency.md:23-36`），BoundaryProfile 又要求
角錨定與共用 edge seed（`design/edge-consistency.md:79-110`）。各面依自身海岸 flood-fill 會覆寫
邊界，確實不相容。下放前須由 parent 全域先算 distance 並傳邊界值，或令面內解服從既定
BoundaryProfile；不能各面自行算。

## 排序建議（一次只做一項）

1. **完成 M5.7 domain warp 並以圖裁決。** 最直接打到已量出的「海岸沿板塊直邊」，改動最小；
   不同時改 octave、falloff 或 redistribution。
2. **若海岸自然但內陸仍怪：固定 land mask，以 coast distance 取代陸上低頻高度基底，保留
   plate ridge 與 fBm。** 只比較高度、河網、relief。
3. **若 biome 的河谷濕潤帶仍只有一格：把 river bonus 改成距淡水衰減。** 不取代風／雨影，
   不回饋河流生成。

Redistribution 只在後續高度直方圖證明谷／峰比例不對時再排入；radial island falloff、terraces
與「增加 octave」目前都不做。
