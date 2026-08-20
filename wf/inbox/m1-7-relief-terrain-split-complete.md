# 信：M1.7 terrain／relief 分離裁決完成

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**基準**：`061f75a`（`main`）

## 完成內容

- `data/biomes.toml` 已拆成兩份獨立、有序 first-match 表：4 條 `terrain_rules` 與
  3 條 `relief_rules`，各自有最後一條 fallback。
- `TerrainRule` 只含 temperature／moisture／elevation；`ReliefRule` 只含
  elevation／ruggedness。`classify_relief()` 只接受沒有 temperature／moisture 欄位的
  `ReliefClassificationInput`；編譯期 assertion 同時鎖住規則型別與決策輸入。
- 正向回歸探針證明同一個乾燥高山樣本會獨立得到 `terrain.desert`＋`relief.mountain`。
- 沒有新增 terrain／relief 種類，`core/serialize`、`core/zone` 均無 diff。

## 門檻數值零變更自證

對拆分前 `HEAD:data/biomes.toml` 與拆分後檔案抽出所有數字、排序比較：

```text
before: 20 320 500 4500 5200 12500 52000
after:  20 320 500 4500 5200 12500 52000
```

即 `20`／`12500`／`52000`／`4500` 原封不動進 terrain 表，
`5200`／`500`／`320` 原封不動進 relief 表；沒有改任何生成 config。

## 陸地氣候分布

固定 `region_id=0`、128×96、預設緯度 35°；三張各 3,686 個陸地格。temperature 取階段 4
的 `temperature_tenths`，moisture 取階段 5 回灌後、實際供 biome 裁決的 `rivers.moisture`。
分位數採排序後 `floor((n-1)×p)`；上／中／下為 y 方向等分三段，各自在該段陸地格取中位數。

| seed | temperature min/p25/median/p75/max | moisture min/p25/median/p75/max | moisture ≤12500 | 上/中/下溫度中位 |
|---:|---|---|---:|---|
| 515151 | 47 / 127 / 170 / 210 / 240 | 0 / 0 / 483 / 2892 / 37908 | 3481（94.44%） | 221 / 153 / 136 |
| 62208000 | 80 / 160 / 183 / 197 / 217 | 0 / 0 / 0 / 1356 / 28092 | 3455（93.73%） | 198 / 176 / 143 |
| 20260820 | 61 / 110 / 131 / 143 / 171 | 0 / 0 / 0 / 900 / 31644 | 3643（98.83%） | 131 / 136 / N/A（下段無陸地） |

前兩個 seed 有明確的上→下溫度梯度；20260820 的陸地集中在上、中段，無法量下段。

## 拆前／拆後格數

同一批 seed、region 與陸地遮罩。terrain 拆前拆後逐格數完全相同：

| seed | desert 前→後 | grassland 前→後 | tundra 前→後 | swamp 前→後 |
|---:|---:|---:|---:|---:|
| 515151 | 3481（94.44%）→3481（94.44%） | 205（5.56%）→205（5.56%） | 0→0 | 0→0 |
| 62208000 | 3455（93.73%）→3455（93.73%） | 231（6.27%）→231（6.27%） | 0→0 | 0→0 |
| 20260820 | 3643（98.83%）→3643（98.83%） | 43（1.17%）→43（1.17%） | 0→0 | 0→0 |

relief 的變化很大：

| seed | plain 前→後 | hills 前→後 | mountain 前→後 | 非平原總數 前→後 |
|---:|---:|---:|---:|---:|
| 515151 | 3633（98.56%）→1916（51.98%） | 49（1.33%）→1523（41.32%） | 4（0.11%）→247（6.70%） | 53→1770 |
| 62208000 | 3677（99.76%）→2924（79.33%） | 9（0.24%）→723（19.61%） | 0（0.00%）→39（1.06%） | 9→762 |
| 20260820 | 3671（99.59%）→2719（73.77%） | 11（0.30%）→579（15.71%） | 4（0.11%）→388（10.53%） | 15→967 |

## 新 PNG（同 seed 515151、region 0）

- 基底：`out/m1_7_seed_515151_base.png`（1024×768 RGBA）
- 起伏：`out/m1_7_seed_515151_relief.png`（1024×768 RGBA）
- 勢力：`out/m1_7_seed_515151_factions.png`（1024×768 RGBA，基底＋勢力）

我實際看過三張圖：基底仍幾乎全是沙漠；純起伏層已出現連續丘陵帶與清楚的白色山脊，和拆前
零星數格的 relief 明顯不同。勢力圖可正常匯出，未見圖層或 bridge 回歸。

## 兩個問題

### 1. 光把結構拆開，畫面變了多少？

terrain 完全沒變；沙漠仍為 93.73%～98.83%。relief 則從 0.24%～1.44% 非平原，增加到
20.67%～48.02% 非平原；三張圖分別多出 1,717、753、952 個 hills／mountain。也就是 relief
確實是 first-match 耦合造成的結構問題，這輪已解除；terrain 單色問題沒有被掩飾，留待下一輪。

### 2. 濕度動態範圍到底多寬？

數值全幅看似有 0～28,092/37,908，但那是稀疏濕尾；真正的陸地主體非常窄且貼零：三個 seed
的 p25 全是 0，中位數為 0/0/483，p75 也只有 900～2,892，導致 93.73%～98.83% 落在
12,500 以下。我的判斷是**水氣模型沒有在大部分陸地產生足夠的動態範圍**，不只是門檻未校準；
單改門檻能改標籤比例，但仍會把高度集中的近零值切成另一種大面積單色。具體調哪個水氣參數仍由
規劃者裁定，本輪沒有調參。

## 驗證

- 四 target：`cmake --build build --target aetheria_core aetheria_tests aetheria_sim aetheria_bridge --parallel 2`，零警告。
- CTest：110/110 通過；含決定論與 `CoreIsolation.CompileCommands`（core 零 godot-cpp）。
- 階段隔離：改階段 6 moisture bias 後，階段 1～5 hash 全相同，只有 biome hash 改變。
- 十二階段＋populate：554.042 ms，低於 3 秒預算。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- Godot headless editor／主場景：兩者 exit 0。
- 三張新 PNG：皆成功匯出並實際檢視。
- `git diff --check`：通過；未 push。
