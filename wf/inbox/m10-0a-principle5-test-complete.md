DONE — M10.0a 原則五守門已落地，CTest 418/418 通過

## 交付

- `tools/check_principle5.py`：以 Python 3 標準庫掃 `core/` 的 `.h/.hh/.hpp/.hxx`；遮蔽註解、一般／字元／raw literals，忽略前向宣告與空 enum。
- `tests/principle5_allowlist.txt`：以 `core/path.h::Enum | 理由` 記 45 項機制、9 項待清償。
- `cmake/targets_tests.cmake`：新增 `principle5.ContentKindsAreData` 一條 `add_test`。

## 驗收輸出

現況與假 enum 還原後：

```text
$ ctest --test-dir build -R principle5 --output-on-failure --parallel 2
Test project /home/lorkhan/repo/game_dev/aetheria-wt-m10-0a/build
    Start 413: principle5.ContentKindsAreData
1/1 Test #413: principle5.ContentKindsAreData ...   Passed    0.06 sec
100% tests passed out of 1
Total Test time (real) =   0.06 sec
```

負向控制一（臨時加入假內容 enum，exit 8）：

```text
1/1 Test #413: principle5.ContentKindsAreData ...***Failed    0.06 sec
principle5 check failed:
  core/principle5_negative_control.h:5: enum class FakeTerrainKind has enumerators but is not allowlisted; add it to [mechanism] only for fixed state/behavior, or to [debt] for a content kind that must be data-driven
0% tests passed, 1 tests failed out of 1
```

刪除臨時 header 後重跑：`1/1` 通過（0.06 sec）。

負向控制二（臨時刪除待清償 `CohortRole` 列，exit 8）：

```text
1/1 Test #413: principle5.ContentKindsAreData ...***Failed    0.06 sec
principle5 check failed:
  core/site/site_combat.h:17: enum class CohortRole has enumerators but is not allowlisted; add it to [mechanism] only for fixed state/behavior, or to [debt] for a content kind that must be data-driven
0% tests passed, 1 tests failed out of 1
```

還原該列後重跑：`1/1` 通過（0.06 sec）。掃描器另有明文反向檢查：待清償 enum 消失時要求從白名單刪除。

全套：

```text
$ ctest --test-dir build --output-on-failure --parallel 2
418/418 Test #415: SimWorldgen.DumpAndVerify ... Passed 5.01 sec
100% tests passed out of 418
Total Test time (real) = 47.04 sec
```

基線 417，本輪新增一條，總數為 418，未減少。

## 機制 enum 與理由

- `FactionAiLod`：AI 精度層級；`BattleAssessment`：評估結果；`PeaceTerms`：和談操作。
- `DungeonDepthSource`：深度演算法分派；`LocalCombatSide`：二方席位；`LayerCombatState`：戰鬥狀態。
- `LocalMoraleEvent`：士氣轉移訊號；`ExplorationStepResult`：導航結果；`DoorState`：門狀態。
- `LocalPathStatus`：尋路結果；`LocalPathInteraction`：路徑操作；`ArgumentKind`：參數解讀模式。
- combat `Outcome`：戰鬥終局；`TrapCheckAttribute`：核心屬性路由；`TrapDisarmMethod`：拆除機制。
- `PowerSourceKind`：力量取得機制；`SpellScale`：地圖層級；`SiteFillZone`：填充分區桶。
- `SiteQuotaDriver`：配額公式驅動；`UndergroundKind`：地下生成路線；`WorldConnectionType`：portal 解析法。
- `PlayableBattleChoice`：戰鬥流程選擇；`PlayableResidence`：駐留層級；`PlayableEventKind`：UI 協定標籤。
- `HookKind`：腳本掛點；script `Outcome`：腳本勝負狀態；`ZoneDecodeMode`：codec 模式。
- `SiteMigrationObjectKind`：遷移 variant 標籤；`CohortFormation`：戰術狀態；`CohortFacing`：格網朝向。
- `ContactArc`：接觸幾何；`SiteZoning`：投影空間狀態；`BuildingState`：建築生命週期。
- `ProceduralBuildingDamage`：損壞狀態；`BoundarySide`：方格拓樸；`CombatLayer`：戰鬥縮放層級。
- `FateOutcome`：命運結果；`FateNarrativeKind`：輸出紀錄標籤；`FateResolutionPath`：解析路徑。
- `TurnStage`：回合階段；`SettlementTier`：有序成長位階；`Significance`：observer 門檻。
- `PlateBoundaryType`：板塊關係；`LodLevel`：zone LOD 狀態；`ZoneLevel`：ZoneKey 拓樸層級。

## ⚠ 第九項內容 enum

實掃發現 `core/local/local_tiles.h::OverlayId`：列出 Road、Vegetation、Stone、ScatteredObject、Furniture、Stairs；其中道路更是原則五明列的資料種類。因此已標紅加入待清償段，既有八項未漏。

灰區曾透過 `.codex-inbox/m10-0a.ask` 詢問；未收到回覆，依任務書既定八項＋上述明確第九項完成。其餘灰區按固定演算法分派／狀態列機制，理由已逐條寫入白名單與上節。

## 模組與依賴（原則九）

- 掃描器在 `tools/`，只依賴 Python 標準庫 `argparse/dataclasses/pathlib/re/sys`，不依賴 core、Godot 或第三方套件。
- 白名單在 `tests/`，是掃描器的唯讀輸入；不被玩法模組引用。
- CMake tests 區段只有一條 `add_test`，由 CTest 單向呼叫掃描器；未改 `core/`、`bridge/`、`godot/`、既有測試或序列化。
