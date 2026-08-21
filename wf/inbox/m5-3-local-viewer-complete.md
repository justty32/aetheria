# M5.3 完成回報 — Local／Site 檢視器

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

**原任務書**：[m5-3-local-viewer.md](done/m5-3-local-viewer.md)

## 結果

已新增 `aetheria_sim gen local` 與 `gen site`。兩者直接讀既有純 C++ 生成結果，使用
`sim/` 自有的無外部依賴 RGB PNG writer 輸出 513×513 PNG；沒有修改任何 `core/`
生成邏輯、`design/` 或 Godot 狀態。`aetheria_core` 的依賴邊界未變。

固定見證可用一行重現：

```sh
./build/aetheria_sim gen local --site-seed 0x5A17 --zoning residential --z all --output out/local-city
```

另三組驗收輸出：

```sh
./build/aetheria_sim gen local --site-seed 0x5A17 --zoning open --z 0 --output out/local-wilderness
./build/aetheria_sim gen site --site-seed 0x5A17 --kind city --output out/site-city
./build/aetheria_sim gen site --site-seed 0x5A17 --kind wilderness --output out/site-wilderness
```

Local 每個選定 z 層各輸出 `ground`、`edges`、`rooms`、`occupants` 四張；`--z` 接受
`-1`／`0`／`1`／`all`，`--z=-1` 已單獨實跑。住宅、商業與 open zoning 都已實跑。
Site 城市輸出 ground／blocks／roads／edges，荒野輸出 ground／roads／content。

## 固定輸出與 edge 表現

`site_seed=0x5A17` 住宅實測為 15 棟、124 間房、108 扇門、51 扇窗、66 件家具、
68 名統計居民與三個 z 層。繪圖用副本會以既有入口具象化各屋居民，讓 occupants 圖能同時
顯示家具與居民；不改核心結果或存檔。

牆／門／窗都按 `LocalTiles::edges` 畫在格線座標：黑＝牆、橘＝門、青＝窗；沒有把牆塗成
一格。家具是青點、居民是桃紅點、樓梯是黃點。Site 同樣按 edge 畫；黑＝城牆、橘＝城門、
紫＝塔、青＝護城河。城市固定圖有 32 街廓、674 道路格、488 個牆 edge 記錄、8 個牆門
記錄（雙牆環）與 317 棟程序建築。

⚠ M5.2 回報把固定輸出記為 65 件家具；目前 literal `0x5A17` 命令實測為 **66**，但
15／124／108／51 與回報一致。這輪未改生成邏輯，也未為湊數修改結果，請規劃者裁定舊回報
的 seed 名稱／家具數是否需要更正。

## 我自己看圖後的判斷

- **Local 房間**：z=0 是 15 棟沿四邊排列的排屋，中間留出很大的完整內院；房間長寬有
  明顯差異但全是正交矩形，比例大致可住。z=−1／+1 只在部分房屋出現，稀疏但不是輸出空白。
  整體規整程度偏高，像規劃式排屋，而不像自然長成的老城。
- **門窗**：橘色門多落在房間分隔邊中段，外牆也看得到入口；沒有門佔一格或門浮在房中央。
  但 124 間配 108 門視覺上很密，每個房間幾乎都是同型「矩形＋置中門」，重複感強。
  青色窗沿外牆排列合理，沒有看到整間封死。
- **家具／居民**：各層點位都落在房內；z=0 分布廣，地下室與樓上隨存在的房屋變稀疏。
  大片黑區是刻意的內院／沒有該垂直層，不是 renderer 漏畫。
- **Local 路線 B**：森林散布與路線 A 一眼不同；地面沒有整片空白。但道路與河流從四邊
  各自延伸約半張圖後，在不同座標突然中止，沒有互相接起來，看起來不像可用的路網／水系。
  這是既有生成結果，不是畫圖造成；依任務書只回報、不修。
- **Site 城市**：32 個街廓大小差異明顯，遞迴二分形狀看得出來；主幹與次級道路連通，
  雙牆、城門和道路交點對得上。部分大街廓留有很大的純綠空地，317 棟建築則大量貼路排成
  規則帶狀，城市仍偏棋盤式，但沒有 97% 單色或整張空白。
- **Site 荒野**：粗糙地、植被、資源、遭遇與 portal 都有內容，和城市明顯不同；但固定輸入
  的道路／河流明顯沿西、南邊界繞行，形狀過度貼邊，也建議後續由規劃者判斷是否屬生成缺陷。

## 驗證

- `cmake --build build --parallel 2`：通過（完整 target）。
- `ctest --test-dir build --output-on-failure`：223/223 通過，73.35 秒；
  `CoreIsolation.CompileCommands` 與新 `SimViewers.GeneratePng` 均通過。
- 新 CTest 以 `0x5A17` 跑城市／荒野兩條 Local 路線與兩種 Site，核對 23 張必要 PNG、
  PNG signature，以及固定 15／124／108 見證。
- `./build/aetheria_sim --tick 62208000`：通過，抵達 year 3 / season 1 / month 1 / xun 1。
- 實際開圖檢查三個 Local z 層、Route B、Site 城市／荒野；所有檔案均被辨識為
  513×513、8-bit RGB、non-interlaced PNG。
