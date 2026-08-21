# M5.8 完成回報 — 治理釋回不變式

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者

## 結論

只修改 `tests/worldgen/faction_metrics_test.cpp`。原本把釋回前後接觸格數與成本寫死為相等，
現改為機制真正應守住的關係；沒有修改玩法、資料或設計文件。

新斷言如下：

- 無主陸地明確落在 `>1%`、`<50%`，同時擋住幾乎全有主與幾乎全無主。
- post-release 接觸格必須大於 0，且不得多於 global 接觸格。
- `global - post-release > 0`，固定 seed 12345 必須證明釋回確實有作用。
- 接觸格保留率至少 25%，避免只看倖存接觸格的平均成本而漏掉接觸面崩塌。
- post-release 接觸格成本總和不得高於 global；不再要求兩者完全相等。

沒有把 96、195、401 或 763 寫進斷言。接觸格保留率採用既有 M1.15 紀錄的判準；
目前 canonical seed 為 `96/195 = 49.231%`，25% 下限保留足夠緩衝，又能攔截嚴重的
倖存者偏差。

## 三個 seed 量測

固定 128×96、region 0、陸地 3,686 格；三 seed 逐一替換測試 seed、重建並收數，之後已
恢復正式 seed 12345。

| seed | 無主陸地 | 無主陸地比例 | global / post 接觸格 | 保留率 | global / post 接觸格成本 |
|---:|---:|---:|---:|---:|---:|
| 12345 | 411/3686 | 11.150% | 195 / 96 | 49.231% | 763 / 401 |
| 424242 | 76/3686 | 2.062% | 81 / 81 | 100.000% | 322 / 322 |
| 515151 | 979/3686 | 26.560% | 141 / 141 | 100.000% | 585 / 585 |

後兩個 seed 雖有釋回無主陸地，釋回格不在勢力對勢力接觸面，因此接觸格沒有縮減。
所以「接觸格必須縮減」只放在這條既有 canonical seed 12345 測試，不泛化成三 seed 規則。

## 負向控制

暫時把測試量測使用的 post-release owner 換成 global owner，令
`post_release_boundary_tiles == global_boundary_tiles == 195`。正式測試 exit 1，新增斷言確實紅：

```text
Expected: (global_boundary_tiles - faction_boundary_tiles) > (0U)
  actual: 0 vs 0

Expected: (unowned_land * 100U) > (land)
  actual: 0 vs 3686
```

既有 `released == result.factions.owner` 也同步失敗，符合停用釋回的預期。取證後已完整移除
故障注入，正式單測恢復通過。

## 圖與目視判斷

- 三 seed 改前／改後並排：`out/m5-8/before-after-contact-sheet.png`
- 原圖：`out/m5-8/before/seed-*/12-factions.pgm`
- 改後：`out/m5-8/after/seed-*/12-factions.pgm`

逐輪重出第 12 階段勢力圖。目視三組左右完全一致；三 seed 的改前／改後 PGM SHA-256 也
各自相同，證明本輪純測試斷言修正沒有改變世界輸出。負向控制只注入測試量測狀態，不改
玩法產圖，其 seed 12345 圖亦與正式版本雜湊相同。

## 驗證

- `cmake --build build --parallel 2`：通過，所有建置均限制為 2 個平行工作。
- 目標單測：通過；canonical 數字為無主陸地 411/3686、接觸格 195 → 96。
- `ctest --test-dir build --output-on-failure`：227/227 通過。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- Godot fresh editor 首掃 exit 139；依既有規約原樣第二次 editor exit 0，主場景 exit 0。
- `git diff --check`：通過；未動 `core/`、`data/`、`design/`，未 push。
