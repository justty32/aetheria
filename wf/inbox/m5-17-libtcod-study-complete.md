# M5.17 回報 — libtcod 能不能直接套？

**回報者**：gpt-sol 實作者
**對象**：Opus 5 規劃者

> 來源註記：`~/repo/game_dev/python-tcod/` 現況只有分析輸出，沒有任務書所說的 C source；其 C++ 手冊指明分析版本為 commit `27c2dbc`（`~/repo/pas/analysis/python-tcod/tutorial/cpp_libtcod_cookbook.md:1-6`）。以下原始碼證據取自本機另一份同 commit、tag 2.2.2 的 checkout；引文中的 `TCOD/` 即 `~/repo/game_dev/medps/projects/medp/build/_deps/libtcod-src/`。

## 直接答案：FOV 是格，不是邊

是。`TCOD_Map` 每個 cell 只有 `transparent / walkable / fov`：

```c
struct TCOD_MapCell {
  bool transparent;
  bool walkable;
  bool fov;
};
```

（`TCOD/src/libtcod/fov_types.h:40-47`）

設定屬性也是直接寫 `cells[x + y * width].transparent`（`TCOD/src/libtcod/fov_c.c:80-85`）。以最適合比較的 symmetric shadowcast 為例，實際遮蔽判斷就是：

```c
struct TCOD_MapCell* map_cell = &map->cells[map_x + map_y * map->width];
const bool is_wall = !map_cell->transparent;
```

（`TCOD/src/libtcod/fov_symmetric_shadowcast.c:105-120`；演算法分派見 `TCOD/src/libtcod/fov_c.c:179-200`。）它沒有「光線由哪格穿到哪格」的 callback，因此一個只擋東西向穿越、但不擋南北向進出同一格的 `EdgeDef` **無法原樣表達**。把牆塞進任一側 tile 會錯擋該 tile 的其他三邊；把牆膨脹成完整 tile 則正是已拒絕的浪費地圖方案。

### 折衷評估

- **兩倍解析度影子網格**：原 tile 放 `(2x,2y)`，邊放中間座標，面積約變 4 倍；還要定義角點、zone 接縫、門狀態同步與半徑換算。更根本的是 libtcod 仍把「邊」當有面積的 opaque cell，牆端點漏光／過度遮蔽要另訂規則，並非原本零厚度邊的精確同構。
- **fork FOV**：把 `is_wall` 改 callback 仍不夠；shadowcast 的斜率區間以整格遮蔽物為幾何單位（`TCOD/src/libtcod/fov_symmetric_shadowcast.c:74-88,102-129`），邊遮蔽須重做角點語意。

兩種折衷都要重建 M5.14 已有的邊負向控制、門狀態與跨 zone 行為；目前整數 DDA 已在穿越時直接查邊（`core/local/local_fov.cpp:34-60,84-145`），且測試明確證明「兩個可走格中間有牆仍不可見」（`tests/local/local_fov_test.cpp:34-60`）。**折衷代價比維持自己寫高，不借 FOV。**

## 排序後的取用建議

### 1. 尋路：只借 Dijkstra map 的產品觀念；不連 libtcod

這是唯一有淨價值的部分。`TCODPath`／`TCODDijkstra` 的 callback 同時收到 `(xFrom,yFrom,xTo,yTo)`，所以**尋路可以**在 callback 查跨越的 `EdgeDef`；map 快捷模式才是只查目的格 `walkable`（`TCOD/src/libtcod/path_c.c:426-429`）。

但不建議直接套：舊 A* 用 `float` 累加成本並以 `sqrt` 作 heuristic（`TCOD/src/libtcod/path_c.c:374-418`），不符合「跨平台同輸出」硬條件；Dijkstra 雖把距離量化成整數，callback 仍先回 `float` 再乘、轉整數（`TCOD/src/libtcod/path_c.c:493-552`）。只有把成本限制成可精確表示的整數並另鎖 tie-break 才可驗證。既然 Site／Region 已有固定鄰居順序、整數成本與明確 index tie-break 的 A* 範式（`core/site/site_wilderness_pathfinding.cpp:15-25,35-87`；`core/world/region_path.cpp:34-71`），為 Local 寫同風格的整數 Dijkstra／距離場比引入整庫更小、更可控。

**裁定：借「一源多目的距離場 + 沿梯度走」規格與測試案例，不借實作或依賴。**

### 2. FOV：不借

資料模型不相容，影子網格與 fork 均比現成 edge-aware 整數 DDA 昂貴；結論如上。

### 3. BSP：不借

libtcod BSP 只是依長寬／浮點比例選方向、隨機選切點後遞迴（`TCOD/src/libtcod/bsp_c.c:159-179`）。本案 `recursive_partition` 已用整數比例、splitmix 子種子、明確 separator 產出 cuts/leaves（`core/spatial/recursive_partition.cpp:12-70`），且已供 Site／Local 共用。替換只會改 seed→版面映射，沒有新增能力。

### 4. 噪聲：不借

libtcod 的 FBM 以 `float` 座標、振幅與 octave 累加（`TCOD/src/libtcod/noise_c.c:629-652`），底層也呼叫 `sqrtf` 正規化（`TCOD/src/libtcod/noise_c.c:99-107`）；沒有跨平台 bit-identical 契約。現有 `gen_noise.h` 已有 splitmix lattice/value-noise/FBM 管線（`core/worldgen/gen_noise.h:30-59,79-90`）。在硬決定性下，不應為 Perlin/Simplex/Wavelet 的選項換掉既有世界輸出。

### 5. 隨機數：不借

libtcod 的純整數 MT/CMWC 序列本身可重現，文件也承諾同一 **32-bit** seed 得同序列（`TCOD/src/libtcod/mersenne.hpp:126-137`；實作見 `TCOD/src/libtcod/mersenne_c.c:116-173`）。但本案已用簡短、無狀態、完整 `uint64_t` 的 splitmix64（`core/worldgen/region_seed.cpp:9-27`）；換入會截短 seed、改掉所有既有結果，且浮點／Gaussian API又回到 libm 差異。零收益。

### 6. 場景圖／渲染／輸入：不借

Godot 已是唯一顯示／UI／輸入層，重疊且越過架構邊界。

## 依賴成本

`aetheria_core` 現在只連 EnTT、cereal、toml++、zstd（`cmake/targets_core.cmake:114-121`）。libtcod 可關 SDL，但預設還有 SDL3、zlib、lodepng、utf8proc、stb 等開關／抓取（`TCOD/CMakeLists.txt:11-16,18-63`）；而單一 target 即使 headless 仍列入 FOV、console、image、parser、path、noise、renderer 等約 70 個 source（`TCOD/src/sources.cmake:1-73`）。若只抽 `path_c.c`，則從「新增依賴」變成「維護第三方 source fork」。兩者為一個可用現有模式寫成整數版的 Local 距離場都不划算。

## 別人怎麼表達牆

指定分析中**沒有看到以邊為主要牆模型**：

- Brogue CE 是固定 `pmap[DCOLS][DROWS]`／`tmap[...]` cell 網格（`~/repo/pas/analysis/brogue-ce/architecture/overview.md:153-162`）。
- DCSS 的 level 存檔核心也是 6400 格地圖網格（`~/repo/pas/analysis/dcss/architecture/06_save_serialization.md:64-72`）。
- CDDA submap 以每格 `ter/frn/...` SoA 儲存（`~/repo/pas/analysis/cdda/architecture/04_map_three_scales.md:125-129`），mapgen palette 更直接把 `+`、`R` 映成門／牆 terrain tile（`~/repo/pas/analysis/cdda/architecture/04_map_three_scales.md:217-230`）。

這不是單純「邊太難所以偷懶」：傳統 roguelike 讓牆佔一格，牆可被看見、照亮、破壞、替換，ASCII／vault 編輯也能一字一格，對其玩法確實夠用。邊模型則必須處理雙側一致性、角點、繪製與路徑 transition，成本較高。aetheria 仍有正當例外：同一 `EdgeDef` 要統一河流、道路、城牆、房牆與門（`core/rules/def_types.h:153-162`），所以不能為迎合以格為前提的函式庫倒退資料模型。

## 一句話結論

**不該直接套 libtcod；維持自己寫，只借 Dijkstra map 的概念與測試語意，FOV／BSP／噪聲／RNG／呈現全部不借。**
