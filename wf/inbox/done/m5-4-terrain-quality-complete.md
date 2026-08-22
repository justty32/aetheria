# M5.4 完成回報 — 地形生成品質

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者

## 結論

只改 `data/biomes.toml` 三個既有門檻，未加噪聲或改演算法：tundra 溫度上限
`20→80`、swamp 海拔上限 `4500→5700`、desert 濕度上限 `32258→36200`。
三個 seed 的最大陸地類型由 69.6–77.7% 降到 40.7–59.6%；四條規則都實際命中。
222/222 測試、三 seed 重生成與正規化世界雜湊皆通過。

尚有一條驗收**不可達成**：Ruleset 現有 5 個 TerrainId 包含 ocean，只有 4 個陸地類型，
所以「至少 5 種陸地 TerrainId 各 >3%」在不新增玩法內容時不可能。曾以單一變因試加 taiga，
直方圖達標但 `RulesetLoader` 固定數量測試失敗，且後段勢力邊界校準改變；依本輪範圍撤回，
請規劃者裁定是否另開內容擴充。

## 改前量測

皆為陸格；格式 `min / median / p95 / max`。分類使用 river 階段回灌後的 moisture。

| seed | temperature（0.1°C） | moisture | elevation |
|---:|---:|---:|---:|
| 515151 | 47 / 170 / 229 / 240 | 9420 / 58757 / 65535 / 65535 | 4198 / 4636 / 5854 / 6267 |
| 12345 | 26 / 95 / 129 / 162 | 10903 / 44098 / 65535 / 65535 | 5156 / 5562 / 5880 / 6282 |
| 424242 | 57 / 125 / 156 / 159 | 7666 / 48231 / 65535 / 65535 | 5442 / 5671 / 6532 / 6915 |

根因：tundra `temperature≤20` 在三張都是分布外（0%）；swamp `elevation≤4500` 在
12345／424242 都是分布外（0%）。desert 原門檻有 15.2%／25.7%／22.3% raw hit，
不是死規則；grassland fallback 因前兩條死規則而吃掉 69.6–77.7%。

## 前後直方圖（陸地百分比）

| seed | 版本 | grassland | desert | tundra | swamp | 最大 |
|---:|---|---:|---:|---:|---:|---:|
| 515151 | 前 | 69.642 | 15.220 | 0 | 15.138 | 69.642 |
| 515151 | 後 | 46.310 | 14.840 | 4.124 | 34.726 | 46.310 |
| 12345 | 前 | 74.335 | 25.665 | 0 | 0 | 74.335 |
| 12345 | 後 | 40.695 | 15.572 | 27.238 | 16.495 | 40.695 |
| 424242 | 前 | 77.699 | 22.301 | 0 | 0 | 77.699 |
| 424242 | 後 | 59.631 | 22.572 | 5.724 | 12.073 | 59.631 |

最終每條規則 selected hit 即上表對應百分比，全部非零；門檻位置詳見每 seed 的 `.txt`。

產物（同名另有 `-histogram.svg` 與 `.txt`）：

- 改前：`out/m5-4/baseline/seed-{515151,12345,424242}-lat-35.png`
- 改後：`out/m5-4/round-6-desert-36200/seed-{515151,12345,424242}-lat-35.png`
- 三圖總覽：上述兩目錄各自的 `contact-sheet.png`

## 單一變因各輪效果

1. tundra `20→80`：三 seed 由全 0% 變 4.1%／27.2%／5.7%；冷側出現連續凍原。
2. swamp elevation `4500→5700`：兩個 0% 變 16.5%／12.1%，第三個 15.1%→34.7%；
   濕地仍須 moisture=65535，集中濕潤低地。
3. taiga 實驗：五種陸地皆 >3%，但造成上述兩個跨範圍失敗，撤回。
4. desert `32258→40000`：直方圖通過但勢力校準釋回 2 個邊界格，撤回。
5. desert `→36000`：勢力測試通過但 seed 424242 grass=60.038%，未取巧四捨五入。
6. desert `→36200`：grass=59.631%，勢力測試仍通過，採用。

每輪完整圖、SVG 直方圖與文字統計都在 `out/m5-4/round-*`。

## 緯度效應

同 seed 515151，中心緯度 0°：grass 44.520%、desert 13.972%、tundra 0%、swamp
41.508%；中心緯度 70°：tundra 100%。證據：最終目錄的 `seed-515151-lat-{0,70}.txt`。
緯度訊號非常強；但高緯基底地形過度單一，是後續要靠新 biome 內容裁定的風險。

## 外部參考（實際讀檔）

### RimWorld

- `~/repo/moddings/rimworld/projects/rimworld/RimWorld.Planet/WorldGenStep_Terrain.cs:91-124`：
  高度混合 Perlin、RidgedMultifractal、macro factor 與 continent noise，不是單層噪聲。
- 同檔 `:126-183,265-323`：溫度＝緯度曲線＋高度遞減＋獨立 offset noise；降雨＝Perlin ×
  緯度曲線 × 高度衰減。本體沒做洋流／顯式雨影。
- 同檔 `:200-212,288-306`：swampiness 另混兩層 noise 再受高度／雨量限制；biome 不是
  first-match，而是遍歷 worker 取最高 score。
- `~/repo/moddings/rimworld/projects/rimworld/RimWorld/BiomeWorker_Desert.cs`、
  `BiomeWorker_Tundra.cs`、`BiomeWorker_BorealForest.cs`：以允許門檻加分數競爭，冷／乾／濕
  可漸進交界。我們沒有獨立溫度噪聲、降雨噪聲、swampiness 場或 score 競爭。

### PAS / World-Generator 分析

- `~/repo/pas/analysis/World-Generator/architecture/Level3-地形與氣候演算法.md:23-30`：
  緯度溫度後再疊 simplex noise。
- 同檔 `:32-43`：逐列風掃描，海上補水、山脈耗水形成雨影，再做 3×3 平滑與 rainfall noise。
- 同檔 `:45-68`：高度×溫度×雨量三維查表，列出 15 類 biome；但文件也指出多個分支未完成。
  我們已有緯度、高度 lapse、緯向風與雨影，缺的是 coherent 氣候噪聲與足夠 biome 內容；
  因量測顯示首要問題是死門檻，本輪沒有照參考再加 noise。

兩邊都沒有可採信的洋流機制證據。

## 看圖判斷與驗證

改前確實像單色溫帶群島；改後同一陸塊上能讀出內陸乾帶、冷側凍原、濕潤低地與草原，
且原有海岸、山系／丘陵陰影未被破壞。仍只有四種陸地底色，高緯 100% tundra，不能宣稱已達
完整星球 biome 豐富度。

- `cmake --build build --parallel 2`：通過
- `ctest --test-dir build --output-on-failure`：222/222 通過（75.64 s）
- `aetheria_sim gen verify --seed 515151 --iterations 3`：通過
- 正規化 world hash 連跑兩次：`15854381691073937971`（4 zones）
- Godot headless：fresh 首掃依已知情形 exit 139；原樣第二次 editor 與主場景皆 exit 0
