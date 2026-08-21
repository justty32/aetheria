# M5.5 完成回報 — Local 路網、地表紋理、聚落形態

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

**任務書**：`wf/inbox/m5-5-local-quality.md`

## 結果

三項均完成；未改 `design/`、`core/worldgen/`、`data/biomes.toml`，未使用 A*。

### ① crossing 路網

- 將原本「每側各畫半張圖」改為同類 crossing 以曼哈頓折線匯入共同樞紐。
- 兩個以上 crossing 必須抵達另一個 crossing；單一 crossing 會形成 5×5 明確終點並留下 `LocalPathEndpoint` 語意。
- 新增 `count_unresolved_open_paths()`，沿實際 ground／overlay 做 flood fill；生成結束時斷頭非零會直接失敗。
- 固定 viewer 見證有道路 crossing 2、河流 crossing 2，改後斷頭數 **0**。

同 seed `0x5A17`：

- 改前：[ground](../../out/m5-5/00-baseline/wilderness/local-open-z0-ground.png)、[occupants](../../out/m5-5/00-baseline/wilderness/local-open-z0-occupants.png)
- 只改路網後：[ground](../../out/m5-5/01-routes/wilderness/local-open-z0-ground.png)、[occupants](../../out/m5-5/01-routes/wilderness/local-open-z0-occupants.png)
- 最終：[ground](../../out/m5-5/final-a/wilderness/local-open-z0-ground.png)、[occupants](../../out/m5-5/final-a/wilderness/local-open-z0-occupants.png)

我看圖的判斷：東西河流現在完整接通；南北道路雖因 crossing 位置不同而有轉折，但都匯入同一節點，不再有憑空停止的半條線。折線很硬，符合本輪「直線＋轉折」裁定，日後美術層可再柔化。

### ② ground 紋理

加入 seed 決定的 4×4 小尺度斑塊噪聲，再以 tile 細噪聲在草地中混入泥、石；邊界套用與河道在其後執行，所以不破壞跨 zone 接邊。

同 seed、同路網：

- 紋理前：[ground](../../out/m5-5/01-routes/wilderness/local-open-z0-ground.png)
- 紋理後：[ground](../../out/m5-5/02-texture/wilderness/local-open-z0-ground.png)

最終 4096 tiles 分布：草 **2969（72.5%）**、泥 **720（17.6%）**、水 **246（6.0%）**、石 **161（3.9%）**。相異值 4 種，沒有 99:1。改前只有草 3898、河水 198，非河地表全為單色。

我看圖的判斷：現在一眼可見細碎土斑與少量石地，仍由草地主導；不是把整張圖換另一個底色。斑塊偏診斷圖風格但已有足夠地表訊號。

### ③ 聚落形態

- 不動共用的 `partition_rect()` 遞迴二分，只改房屋 footprint 排列。
- 同排房屋間保留 1 tile 巷隙，側屋與上下排也保留橫巷；加一棟偏心內側房，中心見證點維持開放。
- 外門位置改由 house／floor seed 決定，不再一律置中。

同 seed `0x5A17`：

- 改前：[edges](../../out/m5-5/00-baseline/residential/local-residential-z0-edges.png)、[rooms](../../out/m5-5/00-baseline/residential/local-residential-z0-rooms.png)
- 最終：[edges](../../out/m5-5/final-a/residential/local-residential-z0-edges.png)、[rooms](../../out/m5-5/final-a/residential/local-residential-z0-rooms.png)

量測：房屋 **15 棟**；footprint 連通塊 **1 → 15**；封閉空地 flood fill 為 **0 tiles（0%）**，原本內院已由巷道連到外部；置中外門 **15/15 → 4/15**。既有規模仍為地面房間 60、全層房間 124、門段 108。

我看圖的判斷：不再是一圈連續隔間；每棟輪廓可分辨，左右與上下都有貫通巷道，偏心內側房也打破修道院式對稱。中央仍偏空，但已是可進出的街巷／空地，不是封閉四方院。

## 負向控制

在乾淨實作上暫時把 `x=16` 整列河流 ground 還原，真的切斷單 crossing 到明確終點的路。指定測試 **exit 1**，紅燈內容：

```text
C++ exception with description "Local 路線 B 產生斷頭 crossing：1"
[  FAILED  ] LocalGeneration.RouteBIsDeterministicNonEmptyAndUnderBudget
```

注入已移除，重建後全綠。

## 效能、決定性與驗證

暖機後 min-of-5（Debug）：

- 路線 A：**1.15857 ms**；這趟生成 houses=15、rooms=124、doors=108、furniture=88、entities=88。
- 路線 B：**0.330969 ms**；這趟生成 tiles=4096、road_paths=1、river_paths=1、scatter=323、objects=3、occupants=0，並執行 crossing flood-fill 驗證。

決定性：同 seed 兩次輸出在 `out/m5-5/final-a/`、`out/m5-5/final-b/`，8 張 PNG 的 SHA-256 逐張一致。代表值：

- wilderness ground：`20f90bd133d4bc198dcf695938bfc7ff626995c647c65573607c125c56a3a503`
- residential edges：`1121e1a5308eedcdb32217f18d719cea2a5b2950fe2bff6c65636e6244da83ca`
- 路線 B normalized hash：`15820055652046872312`
- 路線 A normalized hash：`13020502873653201033`

驗證結果：

- `cmake --build build --parallel 2`：通過。
- `ctest --test-dir build --output-on-failure`：**223/223 通過**。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- Godot headless editor 與主場景：皆 exit 0。
- `git diff --check`：通過。
