# 種類資料化盤點：原則五的現況與打掉／補的裁定

[本層入口](README.md)｜← [runtime-injection.md](runtime-injection.md)｜[principles.md](../principles.md) 原則五｜[INDEX](../INDEX.md)

> **[原則五](../principles.md)：種類是資料，不是 enum。**
> 「`enum class TerrainId : uint16_t {}` 是合法的（強型別防混用），
> 但**枚舉子一個都不准列**。」
> 原則五的權威文件是 **[definitions.md](definitions.md)**（種類一律不寫死 enum，
> 改資料檔 def + 下標）與 [definitions-layout.md](definitions-layout.md)。
> 這份是它的實際盤點——[runtime-injection.md](runtime-injection.md) 的
> 「可編輯的沙盒」要成立，這裡列的東西就得先資料化。

## 原則五的現況：18 對，數個錯

**做對的**（空 enum + TOML def id，共 18 個內容型別）：
`TerrainId`／`ReliefId`／`FeatureId`／`EdgeId`／`GroundId`／`BuildingDefId`／
`CityBuildingDefId`／`FurnitureDefId`／`WorldConnectionId`／`DamageTypeId`／
`SchoolDefId`／`TenetDefId`／`DeityDefId`／`RaceDefId`／`TreatyDefId`／
`CasusBelliDefId`／`PowerBreakthroughDefId`／`TrapDefId`

**做錯的**：`CohortRole`（兵種）、`PersistentBuildingType`（建築，2026-08-23 M8.3 新增）、
`FactionGoal`／`FactionActionKind`、`DungeonArchetype`／`TrapKind`／
`EmergentQuestKind`／`LocalRoomKind` 等。

⚠ **不是所有 enum 都違反原則五**：狀態與機制的 enum（`BuildingState`、`LodLevel`、
`TurnStage`、`DoorState`、`CohortFacing` 等）是合法的。
原則五管的是**內容種類**：地形、河流、道路、建築、單位、事件。

## 打掉 vs 補

判準：**上面有沒有蓋東西。**

| 對象 | 裁定 | 理由 |
|---|---|---|
| **兵種** | **打掉重做** | M7.2／M7.3 的方陣戰鬥與逐單位戰鬥都建在那四個值上，且已對它們做過三層校準。補進去會讓校準基準變成半資料半寫死 |
| **AI 目標與動作** | **打掉重做** | 四個動作是空的，等於還沒實作；而且 L-Script 承諾的「新 AI 行為」完全沒兌現 |
| **建築型別映射** | 打掉（範圍小） | M8.3 才加的，上面還沒蓋東西 |
| 地城原型、陷阱、任務種類、房間種類 | **補即可** | 都是葉子，上面沒蓋東西 |

## ⚠ 動任何一行之前先做的事

**把原則五變成一條測試，掛進 ctest。**

它現在只是文件裡的一句話，所以 417 個測試全綠也擋不住有人再加一個列舉子——
**M8.3 就是這樣加進來的，而且是規劃者批准的**。

沒有這條檢查，打掉重做完還會再腐蝕一次，而且下一次更難發現，
因為「我們剛重做過」會讓人以為它是乾淨的。
