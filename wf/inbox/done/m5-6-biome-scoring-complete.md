# M5.6 完成回報 — 生態帶計分競爭與死規則攔截

**寄件人**：gpt-sol 實作者  
**收件人**：Opus 5 規劃者

## 結論

Terrain 分類已由 first-match 改為整數計分競爭；每條規則用溫度／水氣／高度的 target 與
scale 算固定點距離，取最高分，完全同分固定由規則下標較小者勝出。沒有浮點比較或 map
迭代順序。relief 表不在本輪裁定範圍，維持 first-match。

新增 `terrain.taiga` 與乾濕過渡帶 `terrain.steppe`。三個參考 seed 都有 6 種陸地類型
各自 >3%，最大類型皆 <50%；高緯 70° 不再是 100% 單一地形。正式死規則測試與真正
負向控制均完成。

因硬性範圍只允許 `data/biomes.toml`，兩個 biome 專屬 TerrainDef 與 Site ground 投影也
放在該檔，由 RulesetLoader 合併載入；未改 `data/terrain.toml`／`site_projection.toml`。

## 圖與看圖判斷

- 前後三 seed 並排：`out/m5-6/before-after-contact-sheet.png`
- 改前：`out/m5-6/baseline/seed-*/biome.png`、`histogram.png`、`histogram.txt`
- 改後：`out/m5-6/round-12-tundra-scale-161/seed-*/`（同上三種產物）
- 每個單一變因共 12 輪圖：`out/m5-6/round-*`

目視結果：seed 12345 改前的連續白色凍原直接貼亮綠，改後白色被拆成較小冷端，外側由
taiga 深綠銜接；三張的沙漠與草原之間都有 steppe 赭綠帶。海岸與 relief 明暗紋理未被
破壞。seed 515151 仍偏濕，swamp 48.37% 接近上限，但圖上仍可辨識草原、steppe、乾帶與
窄冷帶，沒有單色吃掉整張。整體硬邊明顯少於改前。

## 直方圖（陸地百分比）

| seed | 版本 | grass | desert | tundra | swamp | taiga | steppe | 最大 |
|---:|---|---:|---:|---:|---:|---:|---:|---:|
| 12345 | 前 | 40.695 | 15.572 | 27.238 | 16.495 | — | — | 40.695 |
| 12345 | 後 | 12.046 | 3.418 | 15.817 | 21.622 | 25.149 | 21.948 | 25.149 |
| 424242 | 前 | 59.631 | 22.572 | 5.724 | 12.073 | — | — | 59.631 |
| 424242 | 後 | 39.175 | 9.712 | 4.883 | 19.560 | 7.569 | 19.099 | 39.175 |
| 515151 | 前 | 46.310 | 14.840 | 4.124 | 34.726 | — | — | 46.310 |
| 515151 | 後 | 27.971 | 8.003 | 3.011 | 48.372 | 3.147 | 9.495 | 48.372 |

固定 seed 515151、緯度 70° 的 3,686 陸格命中：tundra 2,167、taiga 25、steppe 316、
swamp 737、grassland 441（共 5 類；desert 0），不再是 M5.4 的 100% tundra。

## 死規則與決定性證據

`EveryTerrainRuleHitsAcrossReferenceSeeds` 聚合固定三 seed，逐條以 TerrainDef 字串 id 報錯；
同時逐 seed 斷言至少 5 類 >3%、最大類型 <50%。另有故障注入 fixture 驗證探針本身。

真正負向控制把 taiga 溫度中心暫移到 `-32768`、尺度 `1`，並重出故障圖；正式測試 exit 1：

```text
Expected: aggregate[...] > 0U, actual: 0 vs 0
死規則未命中：terrain.taiga
```

完整輸出與圖在 `out/m5-6/negative-control/test-failure.txt`、`contact-sheet.png`；取證後資料已
還原。`TerrainScoreTieUsesDefinitionOrder` 以正反規則順序證明同分取 def 下標。

- `aetheria_sim gen verify --seed 515151 --iterations 3`：通過。
- 正規化 world hash 連跑兩次：`15854381691073937971`（4 zones），相同。

## 勢力校準變化（請裁定）

固定 seed 12345 前後：

| 指標 | 前 | 後 |
|---|---:|---:|
| global boundary tiles | 121 | 195 |
| post-release boundary tiles | 121 | 96 |
| global / post-release boundary cost | 459 / 459 | 763 / 401 |
| global / post-release boundary avg | 3.79339 / 3.79339 | 3.91282 / 4.17708 |
| unowned land | 121 / 3686 | 411 / 3686 |
| map avg cost | 3.11042 | 3.18964 |
| capital minimum Manhattan | 37 | 42 |

因此既有 `FactionGenerationStage.RealRegionIsCanonicalDistributedAndMeasured` 的「release 前後
邊界格與成本必須完全相等」斷言紅：`96 != 195`、`401 != 763`。依任務書沒有回調地形、也
沒有擅自弱化勢力語意測試，保留給規劃者裁定。

## 驗證與範圍

- `cmake --build build --parallel 2`：通過。
- CTest：226/227 通過；唯一紅燈就是上述勢力裁定點。
- headless CLI：exit 0；Godot fresh editor 首掃 exit 139，原樣第二次 editor 與主場景皆 0。
- `git diff --check`：通過；未動 `design/`、`core/local/`，未 push。

依硬性修改範圍未更新 `godot/region_debug_palette.gd` 與 code map；Godot debug viewer 對新 ID
的正式配色需後續在允許範圍的任務補上。本輪驗收圖由 stage dump 解碼 TerrainId／ReliefId
產生，配色與直方圖皆保存在上述 `out/m5-6/` 路徑。
