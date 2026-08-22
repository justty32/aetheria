# M6.6b 世界觀測真值接線完成回報

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**原任務**：[`done/m6-6b-world-observations.md`](done/m6-6b-world-observations.md)

## 結果

已把治安加入 Site→Region 歸約量表第五列；Site 持久層保存治安四因子、具名 NPC 的
`missing`、地城的 `cleared` 與 `depth`。湧現任務的新真值入口只借用 Region 與已載入
Site，運糧、清剿、尋人、探索均由權威狀態生成；外交真值依禁區只保留唯讀介面。

治安公式、初值與任務門檻全部載自 `data/world_observations.toml`：駐軍 45、巡邏 25、
盜匪壓力 40、流民壓力 10、清剿門檻 40、一次降低盜匪壓力 30、運糧目標 100、
地城最小深度 3。實作沒有同值常數或任務專用 Region setter。未改 `design/`、外交、戰鬥、
AI、命運或 `core/serialize/`，未 push。

## 驗收實測

| 判準 | 實測證據 |
|---|---|
| 治安歸約列可用 | 初始 Site 因子算出 `(45+25)-(40+10)=20`，正式歸約後 Region `city.order=20`。 |
| 清剿真的提高治安 | 盜匪壓力 **40→10**，Site／Region 治安 **20→50**，`reduction_writes=1`；完成後同地點不再生成清剿任務。 |
| 缺席 ≠ 0 | 無 `SiteOrderState` 時 delta `has_value=0`，不改 Region 原值 **73**；觀測到 `(10+5)-(12+3)=0` 時 `has_value=1`，Region **73→0**。 |
| 三個持久值接上 | `PersistentNamedNpc.missing`、`PersistentDungeon.cleared/depth` 直接由 Site 持久層讀取；探索條件為 `cleared=false && depth>=3`。 |
| 沒有第二份真相 | `NarrativeWorldView` 只有 `RegionTiles*` 與 Site／外交 `span`；detector 呼叫期間才建觀測值，任務只保存描述與觀測版本，沒有世界狀態容器、全域快取或 Region setter。 |
| 四種任務吃真值 | 同一權威世界實測：運糧 **1**、清剿 **1**、尋人 **1**、探索 **1**；外交 span 為空，所以戰前情報 **0（介面保留）**。 |
| 存檔往返 | Site 冷往返後治安四因子一致、`missing=1`、`cleared=1`、`depth=3`；Region 冷往返後 `order=43`。 |
| 決定性 | 同一 `NarrativeWorldView` 連續偵測兩次，完整任務 vector 相等，`repeat_equal=1`。 |

運糧既有真結算仍為糧食 **40→100**、運入 60、`reduction_writes=1`。清剿與運糧皆先改
live Site 權威來源，再各呼叫一次 `reduce_live_site_xun()`；過期或錯位觀測會在寫入前拒絕，
歸約失敗則還原 Site 變更。

## 負向控制（真的紅，已還原）

只做一次故障注入：暫時把 `measure_site_order()` 的缺值分支由 `std::nullopt` 改成有值的
`0`。用規定的兩路上限重建後，只執行
`SiteReduction.MissingOrderObservationIsDistinctFromObservedZero`：exit **1**，**0/1 通過**。

- `missing_delta.value<OrderReduction>().has_value()`：實際 **true**、預期 **false**。
- 套用缺席 delta 後 Region 治安：實際 **0**、預期保留 **73**。

沒有重試、換故障值或調整 fixture。故障只使用 0 這一個值一次；還原 `std::nullopt` 後，
同測試輸出 `missing_has_value=0 observed_zero_has_value=1 retained_after_missing=73
overwritten_after_zero=0` 並通過。

## 參數與整合紀錄

平衡參數只採用上述一組，沒有為讓數字好看而試其他值。第一次全套 CTest 揭露 4 個整合
失敗：3 個臨時 Ruleset fixture 漏複製新 TOML；1 個固定 Region tile hash 因第五列新增而
改變。補齊 fixture 後，按失敗輸出的三個確定性新 hash 更新基準
`8963508752675768512 / 14515705340403023595 / 3836747774080975080`，沒有反覆試值；四個失敗
聚焦重跑 **4/4** 綠，最終全套綠。

## 驗證

- `cmake --build build --parallel 2`：正確版完整／增量建置均成功。
- 聚焦 `EmergentQuest.*:SiteReduction.*`：**13/13** 綠。
- 全套 CTest：**309/309** 綠，82.27 秒。
- `./build/aetheria_sim --tick 62208000`：exit 0，抵達第 3 年第 1 季第 1 月上旬。
- Godot 4.7.1 headless editor：首次匯入完成後出現已知 exit 139；依工作流只重跑一次，
  第二次 exit 0。主場景 headless `--quit-after 5`：exit 0。
- `git diff --check`：通過；未修改任何指定禁區或 `design/`。

## 現有測試證不了的事／需規劃者裁定

- 現有 worldgen 尚未自動生成具名 NPC／地城持久物件；本輪證明真實持久型別、任務讀取、
  完成效果與往返，不宣稱完整內容生成管線已供應這些物件。
- 任務要求存檔往返、同時禁止修改 `core/serialize/`。新增 Site 持久欄位會改變現行 v15 Site
  payload；本輪可證明新格式自我往返，不能證明舊 v15 Site 檔可讀。格式版號／遷移需由可
  修改序列化的後續任務處理。
- `core/serialize/normalized_state_hash.cpp` 同屬禁區，因此本輪沒有宣稱新 Site 的治安因子、
  NPC／地城旗標都能改變正規化 world hash；Region 第五列已進既有 typed storage hash。
- 戰前情報只驗證介面邊界，沒有接外交權威資料，符合任務禁區；接線後仍需另驗生成數與過期
  外交觀測。
