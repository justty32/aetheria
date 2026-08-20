# 信：M1.8 水氣模型形狀修正完成

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**基準**：`f6a494f`（`main`）

## 完成內容

階段 4 現在明確使用以下整數模型：

```text
uplift_force = rise × uplift_rain
uplift_rainfall = air × uplift_force / (65535 + uplift_force)
moisture = min(65535, air + uplift_rainfall)
air = ceil((air - uplift_rainfall) × air_retention_percent / 100)
```

- `moisture` 的基底是流過該格的 `air`，抬升降雨是額外加成，不再等於本格降雨。
- 抬升降雨依現有 air 的比例平滑增加，且仍從 air 扣除；有限抬升不會再單格抽乾 air。
- 固定量 `air_decay=1800` 已移除，改為每 8 km 格保留 99%；整數除法向上取整，預設模型會
  漸近到最低可表達水氣，不會在固定第 N 格硬著地。
- 海格仍把 air 與 moisture 重設為 65535。沒有改存檔格式、terrain／relief 種類或任何資料門檻。

## 單一新參數自證

唯一的新參數是 `ClimateGenerationConfig::air_retention_percent`；它取代形狀錯誤的
`air_decay`，沒有保留第二個衰減旋鈕：

```diff
-// ClimateGenerationConfig 是固定點氣候與線性雨影的參數。
+// ClimateGenerationConfig 是固定點氣候與比例衰減雨影的參數。
 struct ClimateGenerationConfig {
     std::uint16_t lapse_tenths_per_km{65};
-    std::uint16_t air_decay{1800};
+    // 每個 8 km 格保留的空氣水氣百分比；99 對應約 800 km 的衰減尺度。
+    std::uint8_t air_retention_percent{99};
     std::uint16_t uplift_rain{24};
 };
```

99 是先由格距決定的尺度：`-8 / ln(0.99) ≈ 796 km`，不是為沙漠比例反覆試出的數值。
新欄位已納入 climate 參數 hash；大於 100 會被拒絕。

## `biomes.toml` 門檻零變更

對 `f6a494f:data/biomes.toml` 與工作樹版本抽出所有數字、排序並 `diff`，無差異：

```text
before: 20 320 500 4500 5200 12500 52000
after:  20 320 500 4500 5200 12500 52000
```

`git diff f6a494f -- data/biomes.toml data/terrain.toml data/relief.toml` 亦無輸出。

## 陸地濕度分布

固定 `region_id=0`、128×96、緯度 35°，每張 3,686 個陸地格。口徑沿用 M1.7：取階段 5
回灌後、實際供 biome 裁決的 `rivers.moisture`；分位數為排序後 `floor((n-1)×p)`。

| seed | min / p25 / median / p75 / max | moisture ≤12500 |
|---:|---|---:|
| 515151 | 9420 / **39876** / 55190 / 65535 / 65535 | 40（1.09%） |
| 62208000 | 17181 / **47311** / 58899 / 65535 / 65535 | 0（0.00%） |
| 20260820 | 2638 / **14234** / 32374 / 56280 / 65535 | 801（21.73%） |

三個 p25 均大於 0。第一版只換比例基礎衰減時，seed 20260820 的 p25 仍為 0；追查發現舊的
`rise×24` 絕對量抬升降雨仍可單格抽乾 air。因此改成上述依 air 的平滑比例，沒有更動 99 或新增旋鈕。

## terrain 改前／改後

同一批 seed、region、陸地遮罩；前值來自 M1.7 完成回報，後值由本輪同口徑重算：

| seed | desert 前→後 | grassland 前→後 | tundra 前→後 | swamp 前→後 |
|---:|---:|---:|---:|---:|
| 515151 | 3481（94.44%）→40（1.09%） | 205（5.56%）→2688（72.92%） | 0→0 | 0→958（25.99%） |
| 62208000 | 3455（93.73%）→0 | 231（6.27%）→3686（100%） | 0→0 | 0→0 |
| 20260820 | 3643（98.83%）→801（21.73%） | 43（1.17%）→2885（78.27%） | 0→0 | 0→0 |

這是只改模型形狀後的原始結果；沒有為比例好看去調 biome 門檻或 99%。

## PNG 與目視判讀

- `out/m1_8_seed_515151_base.png`
- `out/m1_8_seed_515151_moisture.png`
- `out/m1_8_seed_20260820_base.png`
- `out/m1_8_seed_20260820_moisture.png`

四張皆為 1024×768 RGBA，已逐張實際檢視。seed 515151 可見濕潤沿海／迎風區、山後灰黃乾舌與
往內陸遞減；seed 20260820 可見北方大陸西側乾區、山帶附近濕區和向東變濕的大片梯度。
形狀已像雨影與大陸水氣輸送，不再是第 15 格後整片零值。仍有兩個待裁定現象：30° 風帶換向處
有偏硬的橫向接縫，部分沿海／河谷飽和藍面積偏大；本輪依紀律沒有再校準。

## 階段隔離、決定論、效能與完整驗證

改 `air_retention_percent` 99→98 的階段 1～4 hash：

```text
before: 8297723058130890806,389988646467817400,7868073020724404054,5118841762756270027
after:  8297723058130890806,389988646467817400,7868073020724404054,1440290728546450103
```

階段 1～3 完全相同，只有階段 4 改變。決定論由逐階段 same-seed 測試與
`SimWorldgen.DumpAndVerify` 通過。

- **Debug** 十二階段＋populate：653.814 ms，低於 3 秒。
- **Release** 十二階段＋populate：515151 = 59.156 ms、62208000 = 40.772 ms、
  20260820 = 53.808 ms，三次皆低於 3 秒。
- Debug 四 target 以 `--parallel 2` 建置完成，零警告。
- CTest：112/112 通過；`CoreIsolation.CompileCommands` 通過，`aetheria_core` 零 godot-cpp。
- `aetheria_sim --tick 62208000`、Godot headless editor、Godot headless 主場景皆 exit 0。
- 濕度提高後，河流測試實際走到陸地湖泊終點，暴露舊檢查把湖格誤當斷流；只把「非湖泊陸格」
  納入 downstream 河級連續性要求，與原測試允許 sea／lake 終止的語意對齊，未改河流生成行為。
- `core/serialize`、`core/zone` 無 diff；`git diff --check` 通過；未 push。

## 三個問題

1. **`f(air)` 是什麼？** 直接 1:1 映射同為 0～65535 的 air 到地表濕度，再飽和加上抬升降雨。
   這保留既有資料尺度、不引入第二個換算旋鈕；濕空氣流過平地自然得到高值，乾空氣得到低值。
2. **p25 是多少？** 39,876／47,311／14,234，全部大於 0。
3. **梯度像真的嗎？** 大形狀是：迎風較濕、背風較乾、內陸漸乾均可見；但風帶接縫偏硬、局部
   飽和偏多，尚不能說校準完成。這兩項應交下一輪裁定，不在本輪偷調門檻或保留率。
