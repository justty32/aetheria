# M5.18 完成回報 — 地形生成外部對照

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者

## 一句話結論

**該套的是 Freeciv 式「只對陸地、在削頂前做整數 CDF/rank remap」；不該套 libtcod
heightmap 實作，也不該為此把風掃描退回獨立濕度 noise。**

## 1. 怎麼避免場削頂／單一規則吃圖

### Freeciv：有真正的直方圖等化，另有類別配額

`adjust_int_map_filtered` 明說把每格改成縮放後的排名；實作先統計每個整數值的頻率，再做累積
頻率（CDF），最後查表回寫（`~/repo/pas/projects/freeciv/server/generator/mapgen_utils.c:114-172`）。
高度圖生成後立即用它映到固定範圍（`~/repo/pas/projects/freeciv/server/generator/height_map.c:255-260`），
溫度也在緯度範圍足夠時等化後才切四帶
（`~/repo/pas/projects/freeciv/server/generator/temperature_map.c:150-170`）。這是本輪找到唯一可直接回答
「有沒有 rank remap／直方圖等化」的出貨遊戲證據。

Freeciv 更沒有讓 swamp 規則逐格自由競爭：先算 forest/jungle/desert/swamp 的目標格數，再逐類
扣額度；候選不存在便把剩餘額轉給 alternate 類別
（`~/repo/pas/projects/freeciv/server/generator/mapgen.c:470-551`）。各額度由全局 wetness、temperature
換算（`~/repo/pas/projects/freeciv/server/generator/mapgen.c:1498-1519`）。因此某個條件再容易命中，
也不能越過預算吃掉半圖。

### libtcod 與 Luanti：沒有同等保證

libtcod 的 `normalize` 只是把 min/max 線性搬到新區間
（`~/repo/game_dev/medps/projects/medp/build/_deps/libtcod-src/src/libtcod/heightmap_c.c:124-140`），
`count_cells` 也只計數、不重整
（`~/repo/game_dev/medps/projects/medp/build/_deps/libtcod-src/src/libtcod/heightmap_c.c:359-369`）；
**沒有 rank remap**。官方 worldgen sample 確實在降水平滑後 normalize
（`~/repo/game_dev/medps/projects/medp/build/_deps/libtcod-src/samples/worldgen/util_worldgen.cpp:966-1018`），但 min–max 不改
相對分布，更不可能拆開已被夾成 65535 的同值群。

Luanti V7 用 heat/humidity noise 後直接生成 biome
（`~/repo/pas/analysis/luanti/details/03_mapgen_v7_pipeline.md:15-23`），分析中未見配額或分布重整；
它靠噪聲參數與 biome 定義調圖，不是可證明的防吞圖機制。

## 2. 溫度與濕度從哪裡來

- **Freeciv（最簡、且已出貨）**：溫度以緯度為基底，高地最多降 30%，鄰海最多調節 15%，
  然後 CDF 等化再離散化（`~/repo/pas/projects/freeciv/server/generator/temperature_map.c:119-170`）。
  它沒有 aetheria 這種逐格 moisture 場；乾燥候選只看緯度帶與附近海洋量
  （`~/repo/pas/projects/freeciv/server/generator/mapgen.c:174-212`），全局 wetness 再決定各地形額度。
  **更簡單卻穩定的原因是配額，不是氣候更擬真。**
- **Luanti V7**：地貌由多層 value noise 合成
  （`~/repo/pas/analysis/luanti/architecture/level_7_terrain_generation_algorithms.md:16-27`），biome 的溫、濕則是
  noise 場（`~/repo/pas/analysis/luanti/details/03_mapgen_v7_pipeline.md:15-20`）。簡單，但沒有分布上限保證，
  不值得取代既有雨影。
- **libtcod sample**：四向風掃描；水面補水，上坡依坡度降雨並扣空氣含水量
  （`~/repo/game_dev/medps/projects/medp/build/_deps/libtcod-src/samples/worldgen/util_worldgen.cpp:854-920`），
  再加緯度/noise、模糊、min–max normalize
  （`~/repo/game_dev/medps/projects/medp/build/_deps/libtcod-src/samples/worldgen/util_worldgen.cpp:923-1018`）。
  溫度是緯度曲線加海拔降溫，最後用 5×5 溫濕查表
  （`~/repo/game_dev/medps/projects/medp/build/_deps/libtcod-src/samples/worldgen/util_worldgen.cpp:1022-1048`）。
  它與 aetheria 同族且更複雜，沒有
  「更簡單卻更好」的證據。
- **World-Generator** 也是緯度曲線 + 單向風掃描 + 雨影 + 平滑/noise
  （`~/repo/pas/analysis/World-Generator/architecture/Level3-地形與氣候演算法.md:23-46`）；它還有固定 seed
  被時間 seed 破壞的 bug（同檔 `:7-15`），不採。

已掃過其餘指定分析：`generator` 是幾何 primitive 函式庫而非地形生成
（`~/repo/pas/analysis/generator/tutorial/usage.md:3-10`）；CDDA 的世界格只存 terrain id，再由 mapgen
展開（`~/repo/pas/analysis/cdda/architecture/04_map_three_scales.md:140-152,182-207`）；Brogue/DCSS 是
地城／vault 生成（`~/repo/pas/analysis/brogue-ce/architecture/overview.md:9-22`；
`~/repo/pas/analysis/dcss/architecture/overview.md:80-98`）；cultivation-world-simulator 的現有分析只把
世界生成列為未來深掘項（`~/repo/pas/analysis/cultivation-world-simulator/architecture/01_level1_overview.md:144-153`）。
它們沒有可對照的連續溫濕場，故不硬湊結論。

## 3. libtcod heightmap 值不值得借

**不值得接入或移植其執行碼；硬條件下直接淘汰。** 固定 seed 能固定 RNG 起點，但 heightmap 全程是
`float`；雨蝕以浮點 slope 比較決定下一格
（`~/repo/game_dev/medps/projects/medp/build/_deps/libtcod-src/src/libtcod/heightmap_c.c:438-476`），
中點位移也以浮點亂數、乘法與累加遞迴
（`~/repo/game_dev/medps/projects/medp/build/_deps/libtcod-src/src/libtcod/heightmap_c.c:592-642`），
noise 初始化還依賴 `sqrtf`
（`~/repo/game_dev/medps/projects/medp/build/_deps/libtcod-src/src/libtcod/noise_c.c:99-138`）。
本機文件／測試未提供跨平台 bit-identical 契約；浮點差異又會
穿過分支改變後續路徑，所以不符合「同 seed 跨平台一致」。

可借的只有演算法名稱與資料流概念；但眼前需要的 rank remap 它根本沒有。侵蝕、Voronoi、FBM
也不對症，不應藉 M5.18 引入新依賴。

## 給未來地形 session 的排序起點

1. **先做唯一主實驗：保留未夾限的 `uint32/uint64` moisture，連 river bonus 都在寬值上完成，
   再只對陸地做整數 histogram CDF remap，最後一次落回 `uint16`。** 海洋不要進 CDF；固定格索引
   作遍歷／tie 規則。若先夾成 65535 才 remap，41.8% 同值已不可逆，這招會失效。
2. **同次實驗用既有 seed 515151 驗收分布與地形占比**：首要門檻是 saturated ratio 不再成塊、
   swamp 不再由削頂群主宰；再看多 seed 是否仍保留迎背風空間排序。不要同時調 biome rule。
3. **只有 CDF 後 swamp 仍過量，才採 Freeciv 的第二層保險：全圖地形配額／上限。** 這會把目前
   純逐格最佳分規則改成全局配置，成本與語意變動較大，所以排第二階段。
4. **不換 Luanti 式獨立濕度 noise，不整合 libtcod，不先做侵蝕。** 前者丟失現有雨影語意，後兩者
   不解決削頂且 libtcod 不過決定性門檻。

**最可能一招解決病灶的是第 1 項。** 它同時消除「寬計算過早塞進 `uint16`」的資訊損失，並把
swamp 所見的 moisture 從絕對飽和值改成穩定百分位；保留風掃描產生的相對乾濕／雨影排序，改動
又比全局 terrain allocator 小。
