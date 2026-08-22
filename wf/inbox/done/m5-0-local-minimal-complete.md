# 信：M5.0 完成 — Local 最小可展開

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**任務來源**：`m5-0-local-minimal.md`

## 完成內容

- `LocalTiles` 已落地為單一 z 層的 64×64 SoA：`ground`／`overlay`／`occupant`／
  `light` 各 4,096 筆，四向 `edges` 16,384 筆。`LocalPayload` 擁有本層 tiles；本輪只生成
  z=0，沒有建立垂直層機制。
- `local_seed` 實作為 `splitmix64(site_seed ^ (y<<16|x))`，並有公式與座標負向測試。
- Region→Site 與 Site→Local 的 canonical edge/corner、salt、角錨定、剖面噪聲、crossing
  與沿界牆裁決，已抽成 `core/spatial/boundary_profile.h` 的**同一份實作**。兩層只各自提供
  父格網唯讀 adapter。
- 路線 B 為固定單趟：B1 邊界內插與細噪聲、B2 直線承接水／路 crossing、B3 抖動網格
  散布、B4 零星覆蓋物。沒有 A*、鬆弛、回溯或重試到通過。B4 尚未生成玩法實體，
  `occupant` 因而全為 0，沒有放入指向不存在 entity 的假 id。
- `materialize_local_zone`、`load_local_zone`、`rematerialize_local_zone` 是三個入口；純 `load`
  解碼後程序 tiles 為 0 筆，`rematerialize` 才重建為 4,096 格。沒有實作持久層疊加。

## 驗收實測

| 判準 | 實測數字 |
|---|---|
| 接邊一致性 | Local 共用邊 64/64 格逐欄一致；雙側、順序／缺席、四角、方向共 4 測試全綠。Site 同組 4 測試全綠。固定 profile hash：Local `3316258571901256250`，Site `3093732465121518141`。 |
| 骨架只讀慢變數 | `build_open_local_skeleton(LocalSlowVars, seed, Ruleset)`；編譯期證明 `LocalFastVars`（controller/damage/season/hour，共 4 欄）不可呼叫骨架入口。 |
| 生成預算 | Debug，暖機後固定 5 次最小值 `0.172241 ms`（門檻 10 ms）。該趟：4,096 格、道路 1 條、河 1 條、散布 325、零星物件 3、occupant 0。 |
| 決定性 | 同輸入兩次正規化欄位雜湊皆 `4739114420143312074`；只把 seed +1 後為 `18148598112628285314`。沒有用 struct byte 相等作跨歷史判準。 |
| 牆壓 zone 邊界 | 人工在相鄰兩個 Local 的共用邊放 `edge.city_wall`；兩側各讀到 64/64 段牆，實際生成 tiles 的東／西外向 edge 也各 64/64 相同。 |
| 展開生命週期 | 首次展開 4,096 格；純冷載 0 格；同一 snapshot 再走 rematerialize 回到 4,096 格。 |

完整驗證：

```text
cmake --build build --parallel 2           PASS（完整 target，零警告）
ctest --test-dir build --output-on-failure 210/210 PASS（68.59 s）
CoreIsolation.CompileCommands              PASS（aetheria_core 零 godot-cpp）
```

## 共用性負向控制

有效注入：把共用 `kBoundaryEdgeSalt` 從尾碼 `...60F` 暫改為 `...60E`，重建後跑完整
210 測試：**208 綠、2 紅**，恰為：

- Site 側紅 `1` 個：profile hash `3093732465121518141 → 1300569287238713690`。
- Local 側紅 `1` 個：profile hash `3316258571901256250 → 16973926684239142368`。

其餘接邊性質測試仍綠是預期：改 salt 不應破壞「兩側相同」，固定語意雜湊才負責證明實作
真的走過被改壞的共用碼。注入已還原；還原後重建並重跑為 210/210。

另先試過把共用 corner salt 改一位，但 Site／Local 都沒有紅：兩個 fixture 的角鄰格 ground
同質且角高度取平均，corner seed 的 ground 選擇沒有可觀察差異。這次無效注入沒有當作通過，
也成為下節的測試保留。

## 被迫加入的新機制／抽象缺口

沒有新增跨層協調、鄰居查詢或不同生命週期；L1↔L2 的降維裁決與 `load ≠ rematerialize`
原樣複用。實作時撞到三個資料界面缺口：

1. 現有 Ruleset 有 `GroundDef`／`EdgeDef`，沒有 `OverlayDef`。為完成指定 schema，本輪加入
   最小、非持久的 `OverlayId` 列舉。這表示 L1↔L2 的 def 抽象尚未涵蓋 L3 覆蓋物；日後若
   overlay 要資料驅動，請規劃者另行裁定，不在本輪擴張 Ruleset。
2. `localgen.md` 要 feature 驅動 B3，但 `SiteProceduralLayer` 不保存來源 feature。本輪由投影
   入口顯式接收慢變數 `FeatureId`，沒有偷讀快變數。這表示「慢變數分流」抽象成立，但
   L2→L3 的慢輸入載體尚缺一欄。
3. Site 沒有逐格 `structure` 欄，而是稀疏 `ProceduralBuilding` footprint。本輪 adapter 只讀
   footprint 推導該 tile 是否有 structure，路線 B 遇到 structure 立即拒絕；沒有另造第二份
   結構狀態。這是兩層 schema 形狀差異，不需要新裁決機制。

## 現有測試證不了的事

- corner salt 的無效注入證明目前同質 fixture 無法證明 corner seed 真的影響 ground 選擇；
  只能由共用程式結構證明兩層用同一公式。要有偵測力需另造四種 ground 的異質角 fixture。
- B3/B4 只證明位置、數量、決定性與預算，不證明散布的美術品質或密度是否好玩。
- `occupant` schema 已存在但本輪沒有玩法實體，所以未證明 entity 建立／搬移與格內引用一致性。
- 沒有宣稱路線 A/C、垂直層、串流、FOV、持久疊加或 Local→Site 歸約已完成；本輪均未實作。

沒有改 `design/`、沒有 fan-out 子 agent、沒有 push。
