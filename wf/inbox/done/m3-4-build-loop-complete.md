# 信：M3.4 完成 — 城建循環

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**任務來源**：使用者直接交辦 M3.4

## 完成內容

- 新增 `CityBuildState` 作為城市 Site 唯一的持久城建狀態，保存已完成建築、未完成工地、
  人口、糧食／生產庫存、人口小數餘額與旬內時數；荒野不建立此 component。
- `SiteTurnPipeline` 在 `L_FULL` 每小時跑產出、相鄰效果、人口與工地進度；每 240 小時先透過
  既有 `RegionTurnPipeline` 結算 Region，再繼續。建造完成的同一小時把當下完整狀態回填
  Region；同時完工的建築合併成一次絕對值回填，不逐棟重複寫相同結果。
- 建築數值改由 `data/site_build.toml` 載入。目前有住宅、農地、礦場、工坊、廣場；建造時數
  24～72 小時。相鄰判定是矩形 footprint 四向邊接觸，不算斜角。
- 人口每小時由住宅容量、當時糧食支撐率與滿意度共同推進，保留定點小數餘額；沒有住宅
  餘量時不再正成長。Region 的卸載近似也不再是常數，預設以完整城建的 500 bp × 85%
  滿意度，即每旬約 425 bp 推進。
- Site→Region 歸約新增糧食與生產庫存 row，採累積絕對值；重複歸約只覆寫，不會再加一次。
  存檔格式由 v11 升到 v12，版本沿革表已補；舊檔依既有政策直接拒絕，不做遷移。

沒有做 SRPG 循環 B、玩家 UI、城區／荒野生成、M1 管線或 M2 界面機制改造。

## 問題 1：推進 N 旬人口真的變了嗎？

**真的變了，M4 現在有非平凡狀態可比較。** 固定 L_FULL 城市從人口 100 出發：

```text
N（旬）       0    1    5    20
L_FULL 人口  100  103  122   230
```

同一組預設成長假設走 Region 卸載近似也會演化：

```text
N（旬）             1    5    20
L_ABSENT 近似人口  104  123   229
```

20 旬 L_FULL 實跑 4,800 個小時；不是直接把終值塞入。這段的分支證據：

```text
constructions_completed=5
completion_reduction_batches=3
adjacency_triggers_first_xun=360
xun_boundaries=20
food_produced=14256
production_stock=28368
```

五棟建築分別在 24／48／72 小時完成；相同完工時點合併回填，因此五棟對應三次完工回填。

## 問題 2：相鄰加成的兩種擺法差多少？

建築集合完全相同，只改座標，推進同樣一旬：

```text
好擺法：production=1008  satisfaction=85  adjacency_triggers=360
壞擺法：production= 336  satisfaction=40  adjacency_triggers=168
差值：  production=+672  satisfaction=+45
```

好擺法生產是壞擺法的 **3 倍**，不是浮在誤差裡的小修正。`360 / 168` 也證明兩邊都實際
進入相鄰判定，結果不是 fixture 零值造成的假差異。

## 不算兩次、pending 與骨架

```text
一旬逐小時 production=1008
再連續做兩次歸約後 production=1008
persistent_city_build_state=1  save_format=12

冷載 pending：completed=1  pending=1  remaining_hours=12
```

pending 測試先推進 36 小時，確認 24 小時建築已完成、48 小時建築還剩 12 小時；存檔後建立
全新 manager／store 冷載，磁碟中程序層為空，再重算並得到同一狀態。

歷史 M3.2 fixture 的六個慢骨架雜湊仍全是 `8177033870425268075`。城建循環自己的骨架在推進
前後也同為 `16265494740625297413`，逐小時經濟沒有污染慢骨架。

M2.3 三次冷往返重跑的六個正規化雜湊全為 `5673161218292418845`；三次都確認磁碟程序層空、
重算程序層非空、F3/F5 非空，且破壞快取後確實重算。

## 效能與完整驗證

同一個 20 旬（4,800 小時）真循環案例：

```text
Debug    31.591 ms
Release   1.972 ms
```

這是 20 旬整批時間，不是單一 Site 生成時間，沒有拿它冒充 5／30 ms 生成預算。兩種組態均
輸出上述完工、相鄰與旬界計數，證明量測段不是空跑。

```text
cmake --build build --parallel 2                    PASS（全 target，零警告）
ctest --test-dir build --output-on-failure          194/194 PASS
./build/aetheria_sim --tick 62208000                exit 0
godot-mono --headless --path godot --editor ...     exit 0
godot-mono --headless --path godot --quit-after 5   exit 0
CoreIsolation.CompileCommands                       PASS（core 零 godot-cpp）
cmake --build build-release --target aetheria_tests --parallel 2  PASS（零警告）
Release 關鍵 7 tests                               7/7 PASS
```

## 問題 3：M3 還缺什麼？

按這輪明定的 M3 範圍，我沒有看到會阻擋宣告 M3 完成的缺口：城區、荒野、接邊與城建循環都
各有非空真例，人口也終於真的隨時間改變，M4 可以開工。

但有兩件不能被這個結論掩蓋：

1. 現在的城建表只是足以驗證循環與空間差異的最小資料集；完整建築表、維護成本與分區規則
   仍在 `midmap.md`「待細化」，沒有設計就不該由實作者自行補數值。
2. `SiteTurnPipeline` 目前一次推進一個 L_FULL Site。設計允許同時存在很多個 L_FULL；未來世界
   時鐘／串流協調器必須在同一小時批次推進它們，並在旬界只結算 Region 一次。M5 路徑尚未
   存在，本輪無法做端到端驗收；如果到 M5 仍逐 Site 各結一次 Region，會重複推進世界。

M4 仍須用完整態與卸載態的真實 N 旬路徑做等價校準，不能因本輪兩條固定序列接近就宣稱
校準完成。這三項是下一階段／待細化事項，不是本輪藏著沒跑的分支。

本輪沒有 fan-out 子 agent，也沒有 push。
