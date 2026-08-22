# M6.6 湧現任務與敘事呈現完成回報

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**原任務**：[`done/m6-6-narrative.md`](done/m6-6-narrative.md)

## 結果

新增純 C++ `core/narrative/`：五種需求的確定性偵測、拒絕過期／錯位需求的運糧結算、
結構化 i18n 事件與具名參數。Godot 新增可整個釋放再重建的 `event_panel.tscn`，只透過
`AetheriaCore.poll_events()` 讀 core 事件快照。命運判定只放與 M6.4 真輸出同形狀的假資料，
沒有實作三階段演算法。未改 `design/`、戰鬥／外交／序列化禁區或 Ruleset 共用檔。

## 驗收實測

| 判準 | 結果與證據 |
|---|---|
| 需求是真的 | detector 逐類檢查世界觀測值，不抽模板；同一 fixture 另放不缺糧、安全、未失蹤、國力差距過大、已清／過淺的負例，皆不生成。運糧另以 live Site + Region 真值實跑。 |
| 完成後真的變好 | 石橋鎮 Region `FoodStockReduction` **40 → 100**，實際運入 60；過期觀測（任務記 40、世界已 55）會丟 `logic_error` 且保持 55。 |
| 既有歸約通道 | `complete_food_delivery()` 只把 live Site `CityEconomy.food_stock` 加 60，再呼叫既有 `reduce_live_site_xun()`；**reduction_writes=1**。`core/narrative` 沒有 Region setter，也未擴充歸約 schema。 |
| 五種都長得出來 | 運糧 1、清剿 1、尋人 1、戰前情報 1、探索 1；任何一類的負例均為 0。細節見下表。 |
| 事件真的顯示 | Godot 4.7.1 headless 繁中 dump 顯示標題、無名總數及兩條具名故事，全文見下節。 |
| Godot 無玩法狀態 | `event_panel.tscn` 實例整個 `free()`，保留 core facade 後重新 instantiate + `poll_events()`；前後文字逐字比較，`NARRATIVE_REBUILD_MATCH=1`。 |
| i18n | `rg '[一-龥]' godot --glob '*.gd'` **0 筆**；玩家文字在 `en／zh_TW／ja.po`，GDScript 只持 key。 |
| 具名參數 | core 傳 `{event,xun,place}` 字典；繁中標題是「事件→旬→地點」，日文同資料輸出「地點→旬→事件」，見下節。 |
| 負向控制 | 暫改為固定 seed 隨機抽一個模板、完全不讀需求後，運糧世界狀態測試 exit 1；詳見下節。 |

五種生成探針（`kind` 0～4）：

| 任務 | subject key | 實際觀測／門檻 | 生成數 |
|---|---|---:|---:|
| 運糧 | `place.stonebridge` | 糧食 40／100 | 1 |
| 清剿 | `place.old_road` | 治安 20／60 | 1 |
| 尋人 | `person.martha_grocer` | 具名、玩家認識、missing=true | 1 |
| 戰前情報 | `faction.river` | 積怨 800／700；國力 1000／920 | 1 |
| 探索 | `place.deep_ruin` | uncleared；深度 8／6 | 1 |

## Godot 顯示與語序證據

繁中（`--language zh_TW -- --event-dump`）：

```text
事件／戰報
[饑荒·第 3 旬] 石橋鎮
  平民損失 1197 人（12%）
  ▸ 雜貨店的瑪莎 倖存，但鋪子已空
  ▸ 老兵葛倫 死於饑荒
NARRATIVE_REBUILD_MATCH=1
```

同一筆 core 事件切日文，標題語序直接換位且值仍對應正確：

```text
[石橋町] 第3旬・飢饉
  民間人の犠牲 1197人（12%）
  ▸ 雑貨屋のマーサは生き延びたが、店は空になった
  ▸ 古参兵グレンは飢饉で命を落とした
NARRATIVE_REBUILD_MATCH=1
```

## 負向控制（真的紅，已還原）

只做一次故障注入：在 `detect_emergent_quests()` 以固定 seed `0x6E61727261746976` 隨機抽
五種模板之一，完全不讀需求；沒有重試或換 seed。重建後執行
`EmergentQuest.FoodDeliveryChangesTheObservedCityThroughOneReductionWrite`：

- exit code **1**，0/1 通過。
- `tests/narrative/emergent_quest_test.cpp:77` 的 `ASSERT_NE(delivery, quests.end())` 失敗；
  實際沒有運糧任務，因此原本應成立的 **40 → 100、歸約 1 次**完全到不了。
- 故障 patch 已移除；綠版同一測試恢復 40 → 100、`reduction_writes=1`。

## 參數嘗試紀錄

沒有調任何平衡值。五種 fixture 門檻各只寫一組正例與必要負例；負向控制只用上述一個固定
seed，沒有為得到特定紅法換值。命運呈現沿用任務書的 1197、12%、第 3 旬假資料。

## 「像不像真的有意義」的主觀判斷

**比「殺 10 隻狼」好，但目前仍只有半步意義，不算有靈魂。** 好的部分是任務指向可查的
缺口：石橋鎮少 60 糧，做完數字確實補上；「瑪莎真的失蹤」也天然比隨機 NPC 更值得追。
較弱的是探索與清剿：即使門檻是真的，現有模板仍像系統通知，沒有交代玩家與古道／遺跡的
既有關係。五筆裡尋人最像故事，運糧次之，探索最像背景工作。若沒有 observer mark、歷史
因果與完成後的具名後果接進模板，光靠「狀態是真的」仍不足以讓玩家在意。

## 基準落差與現有測試證不了的事

- 任務書稱 `poll_events()` 已存在，但本分支 bridge 只有版本、日期、Region 生成三個方法，
  core 也沒有集中事件 feed；本輪因此補上**指定同名通道**，沒有另開第二條 API。
- 現有持久世界模型沒有「治安」「具名 NPC missing」「地城 cleared/depth」欄位；外交真值又是
  本輪禁區。因此這四種目前驗證的是「給定世界觀測快照後只依真門檻生成」，尚不能證明完整
  遊戲循環已供應這些觀測。不能為補證據而在 Godot 或任務模組另建第二份世界真相。
- 現有 `RegionReductionRows` 只有人口、建設、糧食、工業，沒有治安；所以本輪只完成驗收指定
  的運糧真結算。清剿完成後提高治安仍需規劃者先裁定正式世界欄位與歸約列，否則會違反
  `interface-world-mid.md`。
- 畫面證據由命運同形假資料驅動；M6.4 真實三階段輸出尚未存在，故不能證明真事件已接線。
- 自動測試能證明 key、參數、數字與場景重建一致，不能證明三語文案自然，也不能證明玩家
  會在意這些任務；後者仍需實際遊玩觀察。

## 驗證

- `cmake --build build --parallel 2`：成功（core/tests/sim/bridge）。
- narrative 聚焦：5/5 綠。
- 全套 CTest：**303/303 綠**。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- Godot `main.gd`、`event_panel.gd` 各自 `--check-only`：exit 0。
- Godot 4.7.1 headless editor：首次掃描 exit 0；繁中／日文主場景與文字 dump：exit 0。
