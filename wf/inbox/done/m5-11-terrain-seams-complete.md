# M5.11 完成回報 — 地形分布重整接縫與量測入口

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者

## 結論

高度與濕度各有一個具名 `redistribute` 接縫；正式管線的兩個 overload 都是 identity，
沒有任何非恆等轉換進入產品程式碼。散落在生成實作裡的地形常數已收進 config，既有
biome target／scale 繼續由 `data/biomes.toml` 單點持有。這輪沒有改任何數值或設計文件。

三個參考 seed 的正規化 skeleton／tile 雜湊逐位維持原值，完整測試由 228 增至
232 項且全綠。

## 接縫簽章與位置

公開於 `core/worldgen/field_redistribution.h`：

```cpp
template <typename Stage, typename Params, typename Transform>
[[nodiscard]] Stage redistribute(Stage field, const Params& params,
                                 Transform&& transform);

[[nodiscard]] ErosionStageOutput redistribute(
    ErosionStageOutput field, const ElevationRedistributionParams& params);
[[nodiscard]] RiverStageOutput redistribute(
    RiverStageOutput field, const MoistureRedistributionParams& params);
```

- 高度：`erode_height` 之後、`quantize_elevation` 之前。
- 濕度：`generate_rivers` 完成河流回灌後、biome 分類與 `uint8` tile 量化之前。
- `ElevationRedistributionParams` 與 `MoistureRedistributionParams` 目前都是空參數槽；正式
  overload 使用空 lambda，輸入逐位原樣返回。
- 泛型 overload 讓測試注入區域性的 transform。兩個測試分別對第一格高度加 100、第一格
  濕度加 1，證明恆等路徑 hash 不變、非恆等路徑會改變量化輸出／stage hash。非恆等 lambda
  只存在測試檔，正式管線沒有啟用方式。

未來 rank remap 應在這兩個 params 補整數 LUT 與驗證，並替換正式 overload 的 identity；
不要使用 `pow` 或讓浮點排序成為跨平台決定性的來源。

## 常數收攏

- `HeightGenerationConfig` 現在持有既有 coast warp 波長 `16`、幅度 `15.0`；既有海陸比例
  `target_land_percent = 30` 本來就在同一 config，未搬動也未改值。
- `PlateGenerationConfig` 現在持有 `stage_plates.cpp` 原有的海洋板塊比例、漂移半徑、陸海
  基準高度範圍、邊界判定／效果與擴散衰減常數；所有預設值都是原 literal 的原樣抄錄。
- biome target／scale 已在 `data/biomes.toml`，因此保留資料驅動來源，檔案一個位元未動。
- 新搬入的非預設 plate／height 參數已納入原本所屬的參數 hash group；預設參數 hash 編碼
  保持相容，避免只因常數換存放位置而改變既有世界識別。

## 世界未變證據

以下數值在修改前先由 M5.10 基線執行檔記錄，修改後由固定 regression test 重算：

| seed | skeleton hash（前＝後） | tile hash（前＝後） |
|---:|---:|---:|
| 515151 | `5754128893694281728` | `16344487931467028048` |
| 12345 | `17267498220237237745` | `1588818590191442555` |
| 424242 | `793007085422239155` | `15652735773701661944` |

## 量測子命令

單一 seed：

```sh
./build/aetheria_sim gen terrain-metrics --seed 515151
```

三個參考 seed 可直接重出：

```sh
for seed in 515151 12345 424242; do
    ./build/aetheria_sim gen terrain-metrics --seed "$seed"
done
```

可選參數為 `--region <id>`、`--latitude <degrees>`，預設分別是 `0`、`35`。每次輸出四行：

- `terrain_metrics`：陸格數、海岸陸格數、碎形度（海岸陸格／陸格）、海岸與板塊邊界
  重合格數／比例、河流回灌後陸格濕度等於 65535 的格數／比例。
- `elevation_histogram`、`moisture_histogram`：只統計陸格，含 min／median／p95／max、
  自適應 `bin_width` 與 16 bins counts。
- `terrain_histogram`：各 terrain 的陸格數。

本輪實測三 seed 的 post-river 濕度 p95／max 仍全是 `65535/65535`；飽和率依序為
`0.418068`、`0.199403`、`0.177971`。這只把已知病灶做成可重複量測入口，沒有修正或調參。

## 驗證

- `cmake --build build --parallel 2`：通過；所有建置皆使用此上限，未使用更高平行度。
- `ctest --test-dir build --output-on-failure`：**232/232 通過**（原 228 項全保留）。
- `aetheria_sim gen verify --seed 515151 --iterations 3`：通過。
- `aetheria_sim --tick 62208000`：通過。
- 正規化 world hash 重跑兩次皆為 `15854381691073937971`（4 zones）。
- Godot 4.7 headless editor 與主場景皆 exit 0。
- `git diff --check`：通過；`design/` 無異動，未 push。

## 尚缺接口

這輪刻意沒有替空 params 決定 LUT 尺寸、輸入／輸出域、排名同值處理與插值規則；這些都會
影響生成語意，應由後續專門地形 session 裁定。若後續需要同一命令並列 raw 與 redistributed
分布，還需在接縫前保留只供診斷的快照；目前子命令量的是正式 identity 管線輸出。
