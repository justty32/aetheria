# 任務書 M5.8 — 修一條斷言了缺陷的測試（main 目前是紅的）

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**只動 `tests/worldgen/faction_metrics_test.cpp`。不要碰 `core/`、`data/`、`design/`。**
（M5.7 正在改 `stage_plates.cpp`／`stage_height.cpp`，兩邊不重疊。）

---

## 狀況

M5.5 與 M5.6 各自都是全綠，**合併後** `FactionGenerationStage.RealRegionIsCanonicalDistributedAndMeasured` 紅了：

```
faction_boundary_tiles  96   vs  global_boundary_tiles  195
faction_boundary_cost  401   vs  global_boundary_cost   763
unowned_land 411/3686 (11.2%)  vs  global_unowned_land 0/3686
```

## 裁定：紅的是測試，不是程式碼

那條斷言要求 `faction_boundary_tiles == global_boundary_tiles`——
翻成白話就是**「治理距離釋回不准改變任何東西」**。

它之前會過，只是因為釋回**幾乎是空操作**。專案早先就記過這件事：
`governance_max_cost` 讓無主陸地只剩 **1.55%**，世界第一回合就被瓜分完畢，
當時判斷「要有玩法才能裁定，不是現在調」。

M5.6 加了 taiga 與 steppe，通行成本的分布跟著變，**釋回這才真的開始做事**：
無主陸地 0% → 11.2%，國界 195 → 96。

> **這是往設計意圖走，不是回歸。** 那條等號把「機制失效」寫成了期望值。

## 要做的

把等號換成**真正該成立的不變式**：

| 改成斷言 | 為什麼 |
|---|---|
| `post_release_boundary_tiles <= global_boundary_tiles` | 釋回只會縮，不會長 |
| `post_release_boundary_tiles > 0` | 不能全部釋光 |
| **釋回確實有作用**（`global - post_release > 0`） | ⚠ 沒有這條，機制再度失效時又不會有人發現 |
| 無主陸地比例落在一個明確區間 | 把「幾乎沒有無主地」與「幾乎全是無主地」都擋掉 |

⚠ **不要為了讓測試綠就把數字硬編成 96／195。** 那只是把舊病換一個數字重犯一次。
斷言要寫**關係**，不寫**當下的值**。

⚠ 另外：專案記過一條——**查這組指標不要只看國界數字，會被倖存者偏差騙，
判準是接觸格保留率**。若你判斷該用那個指標，改用它並在回報說明。

## 負向控制

把釋回步驟暫時停用（讓 `post_release == global`），**新斷言必須紅**。
⚠ 要真的紅，並寫出紅的是哪一條、數字多少。

## 回報

`wf/inbox/m5-8-governance-assertion-complete.md`：新斷言的內容與理由、負向控制的結果、
三個 seed 的無主陸地比例與國界數字。

## 規約

- `cmake --build build --parallel 2`｜不准 fan-out 子 agent｜不要改 `design/`｜不要 push｜繁中
