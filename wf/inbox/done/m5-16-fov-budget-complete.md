# M5.16 完成回報 — FOV 每回合預算

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

**對應任務**：[m5-16-fov-budget.md](done/m5-16-fov-budget.md)

## 結論

保留 M5.14 的逐候選格整數 DDA、查邊與格角四邊規則；新增只活在單次
`calculate_fov` 內的有向邊狀態 cache。它以相對 origin 的 `(x, y, side)` 為 key，
同一回合重走同一邊時不再反覆進入 `CrossZoneRuntime::peek_edge`；下一回合會重建，
所以門狀態或 zone 載入狀態不會跨回合陳舊。

`r=16` 暖機後 min-of-5 為 **1.06455 ms**、可見 **797 格**，通過 `< 2 ms`。
建議一般預設維持現有 `r=12`，亮視野／裝備加成的上限定在 **r=16**：直徑 33 已超過
64 格 Local 的一半，再加到 r=24 的遊玩收益有限，且 r=24 仍為 2.89 ms。

## 半徑曲線

同一 Debug build、全亮空圖、origin `(32,32)`；每列暖機一次，再固定量 5 次取最小。
每列可見格均大於 0。優化前、後使用同一測試資料與計時範圍。

| r | 可見格 | 優化前 ms | 優化後 ms | 改善 |
|---:|---:|---:|---:|---:|
| 8 | 197 | 0.425495 | 0.210293 | 2.02× |
| 12 | 441 | 1.29970 | 0.538857 | 2.41× |
| 16 | 797 | 3.08351 | **1.06455** | 2.90× |
| 24 | 1,793 | 9.53760 | 2.89433 | 3.30× |
| 32 | 3,207 | 21.9285 | 6.06197 | 3.62× |

優化前 r 加倍時，r=8→16 為 7.25×、r=12→24 為 7.34×、r=16→32 為
7.11×，確認原本確實貼近 **O(r³)**。cache 降低常數但沒有改變 DDA 的最壞漸近階。

## 熱點與選型依據

對優化前 `r=32` 做暫時性插樁（量完已移除）：

- 候選格 4,225；圓內且有 tile 的射線 3,207。
- DDA 格線步進 84,212 次。
- `peek_edge` 呼叫 92,720 次。

候選格只有數千，真正熱點是每條射線重走相同格線，造成近十萬次 runtime 查邊。
加入單回合 cache 後，同一口徑的實際 `peek_edge` 降為 **5,830 次**（15.90×），
符合任務書的「共用射線資料／早退」手段；不必改演算法，也沒有把查邊偷換成查格。

## 兩條負向控制（真的紅）

### 1. 停用共用結果

先把正式 `< 2.0 ms` 斷言寫入測試，再暫時令 cache 每次都重新查邊。測試紅燈：

```text
LocalFov.RadiusCurveUsesWarmMinOfFiveAndTraversesNonEmptyFields
Expected: minimum < 2.0
  Actual: 3.083724 vs 2
local_fov_radius=16 ... visible_tiles=797
1 FAILED TEST
```

可見格仍有 797，故不是空結果假通過。恢復 cache 後同一斷言為 1.06455 ms。

### 2. M5.14 查邊控制

暫時把遮蔽判定從 `peek_edge/EdgeDef` 改為 `peek_tile/GroundDef::move_cost`。測試紅燈：

```text
LocalFov.PassableTilesSeparatedByWallEdgeDoNotSeeAcross
Value of: visible(result, {kNavigationCenter, {33, 32}})
  Actual: true
Expected: false
fov_edge_negative_control_visible=49
1 FAILED TEST
```

查格 mutant 讓牆後格變可見，可見格由正式版 **35 → 49**。已恢復正式查邊實作，
相同測試回綠。

## 對稱性、決定性與驗證

- 含 11 段牆的 6 個見證點：**36 個有序配對、0 symmetry mismatch**。
- 相同輸入重算六份完整 `FovResult`：逐欄相等，**0 determinism mismatch**。
- `LocalFov.*`：6/6 通過；正式半徑曲線每列都有固定非零可見格數。
- `cmake --build build --parallel 2`：通過，零警告。
- `ctest --test-dir build --output-on-failure`：**249/249** 通過，83.28 秒。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- Godot headless editor 與主場景：均 exit 0。
- `clang-format --dry-run --Werror`、`git diff --check`：通過。

未改 `design/`、`local_materialize.*`、`zone_manager.*` 或 `local_reduction.*`，未 push。
