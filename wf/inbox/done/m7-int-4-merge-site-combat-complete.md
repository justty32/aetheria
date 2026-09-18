# M7-INT-4 Site 戰鬥整合完成回報

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**原任務**：[`m7-int-4-merge-site-combat.md`](m7-int-4-merge-site-combat.md)

## 結果

已以 `git merge --no-ff m7-2-wt` 把 M7.2 Site 方陣戰鬥併入 `m7-int4-wt`。
兩個預期 CMake 衝突全數解決，M7.0 位階來源、M7.1 地城與 M7.2 Site 戰鬥同時保留。
存檔版本維持 **v20**，沒有改演算法、平衡值或 `design/`，沒有 push。

## 兩個衝突的解法

| 檔案 | 解法 |
|---|---|
| `cmake/targets_core.cmake` | 保留 M7.0 `power_sources` 與 M7.1 地城來源；先替 M7.1 `target_sources(aetheria_core ...)` 補上獨立的 `)`，再於檔尾開一個完整的 M7.2 `target_sources(...)`，加入 `core/site/site_combat.cpp`。 |
| `cmake/targets_tests.cmake` | 保留 M7.0 位階來源測試、信仰隔離護欄與 M7.1 地城測試；先替 M7.1 `target_sources(aetheria_tests ...)` 補上獨立的 `)`，再於檔尾開完整的 M7.2 區塊，加入 `tests/site/site_combat_test.cpp`。 |

兩處都逐段解括號切割，沒有用批次「兩邊都取」。正式差異只來自 M7.2 branch 與上述衝突
解法，沒有額外行為修改。

## 負向控制（真的紅，已還原）

只試任務指定的 **+3%** 一個值：暫時在 `simulate_site_battle()` 已算出的
`result.loss_a`、`result.loss_b` 各再套一次 `300` permyriad 的同向偏差，依規約重建後執行：

`CombatScaling.ExpectationParityCoversBalancedRatiosWithoutSuppressingVariance`

CTest 為 exit **8**、**0/1** 通過。紅的是 `all_same_nonzero_direction(...)`：實際
**true**、預期 **false**，錯誤訊息為「Site−Region 的 A、B、總損失三組誤差不得全部同向」。
實測 `signed_errors_site_region_A_B_total` 三值為：

- A：**0.031181**（+3.1181%）
- B：**0.0288209**（+2.88209%）
- 總損失：**0.0300014**（+3.00014%）

三者同正且各自仍小於 5%，證明新增符號護欄確實堵住原本 magnitude 守門看不見的漏洞。
還原兩行注入並以相同指令重建後，同一正式測試恢復 **1/1** 通過；
`core/site/site_combat.cpp` 沒有暫改殘留。

## 驗證

- `cmake --build build --parallel 2`：冷建置與負向控制前後增量建置皆通過，全程固定兩路。
- M7.0：測試 #348–359 與 #386 全綠；M7.1：#360–369、#376–378 全綠；
  M7.2 Site 戰鬥：#370–375 全綠。
- `ctest --test-dir build --output-on-failure`：**391/391** 通過，83.29 秒。
- `./build/aetheria_sim --tick 62208000`：exit 0，抵達第 3 年第 1 季第 1 月上旬。
- Godot 4.7.1 headless editor 連跑兩次皆 exit 0；主場景 exit 0。
- `git diff --check` 與 staged diff check：通過；無未解衝突，`design/` 無異動。
- `core/serialize/zone_codec.h` 仍為 `kSaveFormatVersion = 20`，未自行選號或改版。

## 不得不改的東西

無。除了兩個 CMake 衝突的括號與獨立檔尾區塊外，沒有新增整合修補；負向控制只存在於
驗證期間，已完整還原。
