# M6.2 外交關係模型實作完成回報

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**原任務**：[`done/m6-2-diplomacy.md`](done/m6-2-diplomacy.md)

## 結果

完成資料驅動外交規則、有向四分量矩陣、條約實例、會過期的宣戰理由、持續戰爭事件、
厭戰演化、公開整數和談公式，以及型別／target 雙層隔離的 `FactionView`。未寫 AI 決策、
效用評分、目標庫、均勢、連鎖參戰、調停或戰鬥公式；未修改 `design/` 與任務禁止檔案。

## 驗收實測

| 判準 | 結果與實測證據 |
|---|---|
| 有向 | `relation[A][B]` 單獨寫入；`B→A` 保持四項全 0。對稱化故障注入後測試 exit 1，詳見下節。 |
| 四速率分開 | 資料速率（分母 10000）為好感 1000、恐懼 500、信任 100、積怨 25。相同 1000 衝擊經 10 旬，實際回歸量依序為 **653 > 405 > 100 > 30**。載入器也拒絕相同／錯序速率。 |
| 條約是資料 | 正式資料載入 7 種。測試只在暫存 `diplomacy.toml` 新增 `treaty.hostage_exchange` 一筆，未改 C++，數量 7→8；讀得期限 12 旬、條件 `hostages_available`、可續約。 |
| 理由會過期 | `casus_belli.revenge` 資料期限 36 旬；第 35 旬可用 = 1，第 36 旬可用 = 0，過期理由不能宣戰。 |
| 無理由代價 | 1 對 2 無理由宣戰後，第三方 3 對侵略方 1 的信任變成 **−1500**；反方向與交戰目標不被誤寫。 |
| 厭戰逼向和談 | 無傷亡、永不自行結束的戰爭每旬 +40，單調由 0 前進，**第 25 旬達 1000 門檻**。1000 傷亡同旬為 60，無傷亡方為 40。 |
| 和談公式公開 | 公開 `calculate_peace_leverage()` 純函式；玩家世界入口與受限 AI 入口都呼叫它。戰果 500、自方厭戰 200、對方 700、第三方壓力 100 時，兩路皆得 **1100**，條件推導為割地。 |
| 情報隔離 | `aetheria_faction_ai_objects` 只有 `core/ai/include`；`FactionView` 只持有自方真值與他方估計複本，沒有 World 參考。compile-fail 錯誤見下節。 |
| 決定性／整數 | seed 12345、相同真值與觀測歷史產生逐欄相同 `FactionView`；實值為 target2 `(1104,829)`、target3 `(567,1344)`。新狀態與公式欄位皆為整數，新增檔無 `double`／`std::pow`。 |

## 負向控制（真的紅）

### 對稱化關係矩陣

暫時把 `set_relation(A,B)` 改成同步寫 `set_relation(B,A)`，重建後執行
`DiplomacyRelations.MatrixIsDirectedAndFourComponentsRevertAtOrderedRates`：

- exit code **1**，1/1 測試失敗。
- 預期 `B→A = (0,0,0,0)`；故障後立即為 **(1000,1000,1000,1000)**。
- 10 旬後反向仍為 **(347,900,595,970)**，而非全 0。
- GTest 在方向斷言處報兩次 `Expected equality`。故障注入已還原。

### AI 直接讀世界真值

`FactionAiIsolation.WorldTruthCompileFailure` 以 AI target 的唯一 include path 編譯直接讀真值的
來源，compiler exit 非 0，核心錯誤為：

```text
tests/compile_fail/faction_ai_world_truth.cpp:4:10: fatal error:
core/world/diplomacy.h: No such file or directory
```

CTest 確認失敗原因正是世界真值標頭不可見；若意外編過，CTest 反而會失敗。

## 參數嘗試紀錄

只採用一組數值，沒有為曲線反覆試值：回歸 `1000/500/100/25`、厭戰每旬 `40`、和談門檻
`1000`。第一次和談測試把籌碼 1100 的條件誤期待成附庸化（資料門檻 1200），該測試紅；
修正期待為割地，**未修改參數或公式**。

## 驗證

- `cmake --build build --parallel 2`：成功（完整 core/tests/sim/bridge）。
- 外交聚焦：11/11 綠；數字如上。
- `FactionAiIsolation.WorldTruthCompileFailure`：綠（內部編譯如預期失敗）。
- `aetheria_sim --tick 62208000`：exit 0。
- Godot 4.7.1 headless editor 與主場景：兩者 exit 0。
- 全套 CTest：新增隔離 target 後一度為 283/284，唯一失敗是既有 core TU 計數器少認一個
  OBJECT target；納入計數並拆分情報快照 TU 後重新完整執行，最終 **284/284 綠**（79.94 秒）。

## 現有測試證不了的事

- 本輪任務未要求把外交世界狀態接進 zone 存檔；因此沒有外交狀態跨存檔往返證據。
- 25 旬門檻只證明厭戰單調且有限時間達門檻，不證明長期平衡或多場真實戰爭分布合理。
- 第三方壓力目前是公開公式輸入；壓力來源、AI 何時談與真正結束戰爭屬 M6.3。
- `FactionView` 證明 AI 編譯邊界拿不到真值型別，不證明未來每個 AI target 都已接入（本輪沒有 AI 決策）。
