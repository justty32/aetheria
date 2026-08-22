# 地形生成交接

← [SESSION-LOG](SESSION-LOG.md)

使用者裁定**地形之後單開一個 session 處理**。本 session 到 M5.11 為止只留接口、不再調值。
**根因已經量到，不必重新診斷。**

使用者裁定**地形之後單開一個 session 處理**，本 session 到 M5.11 為止只留接口、不再調值。

**接手就從這裡開始**（一行指令重出全部診斷）：

```sh
./build/aetheria_sim gen terrain-metrics --seed 515151
```

**根因已經量到了，不必重新診斷**——濕度壓在 `uint16` 天花板上：

```
moisture_histogram  min=8833 median=61449 p95=65535 max=65535
  counts=28,67,110,73,104,124,86,94,139,136,137,149,179,213,238,1809
                                                                 ↑ 最後一格佔 49%
moisture_saturated_ratio=0.418      terrain_histogram swamp=1823（saturated=1541）
```

**swamp ≈ 飽和格**。因果鏈是：濕度削頂 → swamp 規則在所有飽和格上勝出 → 吃掉半張圖。
所以 M5.10 只能把 `moisture_scale` 收到 8000、壓到 49.457%，離 guard 只剩 **0.54% 餘裕**。

> ⚠ **這是同族缺陷第四次**。前三次是**門檻落在分布之外**（M1 沙漠、M5.4 的 tundra 與 swamp），
> 這次是**分布本身頂在天花板上**。表現不同，結果一樣：**分類規則失去鑑別力**。

**接縫**（`core/worldgen/field_redistribution.h`，兩個正式 overload 都是 identity）：
高度在 `erode_height` 後、`quantize_elevation` 前；濕度在河流回灌後、biome 分類前。
⚠ 未來放 rank remap **要用整數 LUT，不要 `pow`**（跨平台浮點不決定性）。

### ⚠ M5.18 找到了出貨遊戲的現成解：Freeciv 做了**兩件**我們沒做的事

1. **整數 CDF／rank remap**（`freeciv/server/generator/mapgen_utils.c:114-172` 的
   `adjust_int_map_filtered`）——統計每個整數值的頻率、算累積頻率、查表回寫。
   **高度與溫度都在離散化「之前」先等化**（`height_map.c:255-260`、`temperature_map.c:150-170`）。
   這是本輪找到**唯一**能直接回答「有沒有直方圖等化」的出貨遊戲證據。
2. **類別配額**（`mapgen.c:470-551`）——先算各 biome 的**目標格數**再逐類扣額度，
   候選不足就把剩餘額轉給 alternate。
   ⚠ **這比計分競爭更硬**：計分競爭仍允許某個 biome 到處贏，
   **配額讓「吃掉半張圖」在結構上不可能發生**。

⚠ **libtcod 的 `normalize` 只是 min/max 線性搬移，沒有 rank remap**——
它**拆不開已經被夾成 65535 的同值群**，對我們的削頂完全無效。

**不要**為了這個把風掃描退回獨立濕度 noise（M5.18 的結論）。

**排序建議**在 [m5-9-redblobgames-study-complete.md](inbox/done/m5-9-redblobgames-study-complete.md)
與 [m5-18-terrain-references-complete.md](inbox/done/m5-18-terrain-references-complete.md)；
⚠ 其中一條裁定：**coast distance 在 L1 相容，但各 Site／Local 自行 flood-fill 會覆寫邊界、
與降維裁決鏈衝突**——下放必須由 parent 全域先算再經 `BoundaryProfile` 傳。
