# M10 波次細目（波 3–5）：原則五補課、新內容型別、注入入口

← [runtime-injection.md](runtime-injection.md)（總覽）｜波 0–1 → [runtime-injection-waves.md](runtime-injection-waves.md)｜波 2 → [runtime-injection-inject.md](runtime-injection-inject.md)

## 波 3 原則五補課（R5a‖R5c 並行，R5b 排整合輪）

打掉 vs 補的裁定照 [type-data-audit.md](../../../design/rules/type-data-audit.md)；
每清一項，從 M10.0a 白名單的「待清償」段刪一列。

### M10.5a 兵種資料化（打掉重做）

- **變因**：`CohortRole` 四值 enum（`site_combat.h:17`）→ `data/units.toml` ＋ 空 enum
  `UnitDefId` ＋ `UnitDef`（沿用 18 個做對的型別的形狀）。
- ⚠ **最大的坑是校準**：M7.2／M7.3 的方陣戰鬥與三層校準建在那四個值上。
  任務書必令：資料化後以**同參數值**重跑校準測試，「幅度＋符號」兩組斷言
  （KNOWN-TRAPS 那條 3% 同向偏差）必須原樣通過——**這輪只換載體，不調平衡**。
- **Done when**：四個兵種全在 TOML；加第五種兵種＝只加 TOML＋測試可用它開打；
  校準斷言原樣全綠；`CohortRole` 從程式碼消失。

### M10.5b 建築型別資料化（範圍小，排在波 3 整合輪內做）

- **變因**：`PersistentBuilding::type`（寫死 enum **且被序列化**，`site_projection.h:94`）→
  存字串 id、載入 remap（照地形四件套模式）。M8.3 才加的，上面沒蓋東西，直接打掉。
- **Done when**：存檔裡不再有建築 enum 值；舊 fixture 以 v23→v24 重生成；
  加一種持久建築＝只加 TOML。版本 bump → v24（與 M10.5a 一起打包定版）。

### M10.5c AI 目標與動作資料化（打掉重做）

- **變因**：`FactionGoal`／`FactionActionKind`（`faction_ai.h:16-33`）→ TOML 定義的
  目標與動作表（權重、條件、效果引用）；效果落在一組**具名內建效果**上（先不做 Lua）。
- **同輪兌現地圖可見的行為**（KNOWN-TRAPS：AI 現在在地圖上什麼都不做）：
  四個空動作至少 `Expand` 要真的**建新聚落**——走 M10.4 接好的 TurnEnd
  `queue_materialize` 通道，勢力擴張與勢力注入共用同一條結構變更路徑；
  `Develop` 至少推動 Site development 數字所對應的持久層。其餘兩個可留最小語意，如實標註。
- ⚠ **躲不開序列化**：`FactionMindState` 持久保存 `FactionGoal`，codec 直接寫 enum byte
  並用 `HolyWar` 當上限驗證（`zone_diplomacy_codec.cpp:126-136,261-282`）。
  R5c 本線仍不碰 `core/serialize/`——goal 改存字串 id 的 codec 變更**列給波 3 整合輪**，
  與 R5b 同批進 v24。
- **Done when**：目標／動作全在 TOML；加一種動作＝加 TOML＋（若需）一個內建效果；
  跑 N 旬後 AI 勢力在地圖上**新增了聚落**（機器可驗證）；決定論不破。

## 波 4 新內容型別（三線並行；新增 `.cpp` 進 CMake 只准 M10.6a 做）

版本 bump → **v25 確定**（不是「視情況」）：人物與 inventory 進存檔必動
`AllComponents` 固定順序白名單（`all_components.h:14-26`），且新增 `culture.toml`／
`items.toml` 改了 raws schema。整合輪統一定版。

### M10.6a 人物成為實體

- **變因**：具名人物從「uid＋名字 key」（`site_projection.h:118-129`）長成實體：
  component 組（名字、屬性、所屬勢力、位置、位階），有**可重複呼叫的生成入口**
  （取代 `playable_session.cpp:953-962` 的 uid=9001 寫死示範），可經日誌注入。
- **順手修斷路**：`resolve_encounter` 的 `NamedFateLedger` 是函式內區域變數用完即丟
  （`playable_session.cpp:953`），從未 emplace 進任何 zone registry——接上
  （正確範例：`tests/world/named_fate_test.cpp:268`）。
- **Done when**：注入一個具名人物→出現在世界、進存檔、重放一致；
  named fate 存讀往返後仍在。

### M10.6b 文明／文化 def

- **變因**：新 `CultureDef`（`data/culture.toml`：命名風格、價值觀權重），
  `faction_defs` 加 `culture` 欄位引用它。現有 `civilization.toml` 的 worldgen 常數**不動**。
- **Done when**：注入一個文化 def＋一個引用它的勢力，全鏈通過；加文化＝只加 TOML。

### M10.6c 物品

- **變因**：`ItemDef`（`data/items.toml`）＋ 最小 inventory component（掛在人物／部隊上）
  ＋ 物品實例可經日誌注入。**不碰**擱置中的 unique-objects／mark。
- ⚠ **依賴 R6a**：def／component 可先並行做，但 Done when 要等 R6a 的人物 API freeze
  才能收——任務書明列「R6a 凍結介面前只做到 def＋單元測試」。
- **Done when**：注入一種物品 def＋把一件物品放進某人物的 inventory→進存檔、重放一致。

## 波 5 收束

### M10.7 注入入口（[裁定#2](runtime-injection-decisions.md) 已定：外部檔案先行）

- 形式：把 TOML 片段丟進 `saves/<世界>/inject/`，下個回合尾端吸收、記日誌、
  移入 `inject/done/`；格式錯誤＝整批拒收並報告（影子驗證），**不得半套用**。
- **Done when**：遊戲跑著，外部丟一個檔進 `inject/`，下一旬結束時內容生效且日誌有記錄。

### M10.8 端到端整合驗收

- 照總覽的**完成判準**六條逐條驗（含[裁定#9](runtime-injection-decisions.md)的
  **全型別矩陣**：此時型別數已含兵種／建築／AI 目標動作／文化／物品，比 M10.3b
  的迴圈測試更寬），全部一次過；並量測 root 級結構
  （勢力表、日誌）的大小寫進回報（[裁定#5](runtime-injection-decisions.md)：只量不修）。
- 順手更新：KNOWN-TRAPS 清掉已解的四條（不會存檔、加勢力弄壞存檔、remap 不驗內容、
  FIFO 死代碼）；[milestones.md](../../../design/milestones.md) 補 M10 列。
