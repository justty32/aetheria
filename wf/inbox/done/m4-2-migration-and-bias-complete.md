# 信：M4.2 卸載偏差診斷與骨架遷移完成

**寄件人**：實作 agent（gpt-sol，`~/repo/game_dev/aetheria`）
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`

## 要告知什麼

M4.2 已實作、完整驗證並提交到 `main`（未 push）。

### 1. 偏差不隨卸載次數線性增長

固定總長 20 旬；非零組另固定 10 旬 live、10 旬 absent，只改卸載／重載的分段次數。
人口偏差如下，development／food／production 各組全為 0：

| 卸載次數 | 人口偏差 |
|---:|---:|
| 0 | 0% |
| 2 | -1.28755% |
| 5 | -0.858369% |
| 10 | -1.28755% |
| 20 | -1.28755% |

2／10／20 次完全相同，沒有每次卸載累積的斜率；5 次的差異來自 live／absent 分段相位。
結論：不是殘留的每次卸載取整，而是 Site 與 Region 人口公式／相位本身的差異。依任務要求未修，
請規劃者裁定是否接受或另開校準工作。

### 2. 骨架遷移三種結局都有真實分支證據

`skeleton_hash` 不同時，重載會用新骨架檢查所有現有座標型持久資料
（`PersistentBuilding`、`CityBuilding`、`PendingConstruction`）：

- 合法且不衝突：保留。
- 不合法：以 Manhattan 距離找最近可容納完整 footprint 的合法空位，固定以 y/x 解平手。
- 無合法空位：從 live 物件移出，完整記入「毀於災變」歷史，不再留下非法 live 座標。
- 每次骨架遷移都追加一則含三種計數與文字的敘事事件；事件與毀壞紀錄會跨冷載持久化。

人工把 Region relief 慢變數改掉後冷重載，實際計數：

| 保留 | 就近搬移 | 標記毀壞 | 事件 |
|---:|---:|---:|---:|
| 2622 | 1 | 1 | 1 |

負向控制先確認若直接沿用舊座標會留下 2 個非法物件。另有獨立測試刻意造出
「搬移 3、毀壞 0」，仍產生 1 則敘事事件，避免實作退化成只有毀壞時才出事件。

### 3. M4 尚不能無條件宣告完成的地方

- 「方向無偏」仍有約 -1.29% 的系統性訊號；本輪已判定不是卸載次數造成，但在規劃者裁定
  接受、或另行校準兩套人口公式之前，我不會把這條寫成已達成。
- 完成判準表中的「命運無偏」仍需要具名 NPC；本輪明令不做，因此若 M4 範圍仍包含該列，
  它依然是 open。若規劃者已正式延後此列，則核心生命週期的其餘 M4 判準已具備測試。
- 本輪產生的是 core 可查詢、可存檔的玩家敘事事件；Godot 端尚無玩法事件 UI，實際畫面呈現
  要等 UI 消費端落地。這不影響「遷移不得靜默」的 core 判準，但不是可手動看到的成品 UI。

## 驗證

- `cmake --build build --parallel 2`：通過。
- `ctest --test-dir build --output-on-failure`：202/202 通過。
- `./build/aetheria_sim --tick 62208000`：通過。
- `godot-mono --headless --path godot --editor --quit-after 3`：exit 0。
- `godot-mono --headless --path godot --quit-after 5`：exit 0。

相關測試：`tests/site/site_migration_test.cpp`、
`tests/site/site_unload_equivalence_test.cpp`。
