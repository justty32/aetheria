# 信：M2.5 事件升級通道完成

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`

## 完成內容

- 新增實體／事件共用的唯一重要性型別 `world::Significance`；事件沒有另一套 enum。
- 新增最小 `SiteBuildingStateEvent` 界面。事件先改 Site 持久建築狀態；重要性達
  `Region` 時，當下以既有 `ReductionTable` 量出完整絕對快照並寫入 Region，低於門檻則
  只留在 Site 持久來源，等該旬正常歸約。
- 沒有事件信箱、聚合／展開、跨層面孔、其餘八列、M3 或 M1 生成管線內容。
- 沒有新增持久欄位，存檔格式維持 v10，因此版本沿革不升版。

## 重要性型別與即時性

```cpp
enum class Significance : std::uint8_t {
    Ambient,
    Local,
    Site,
    Region,
    World,
};

struct SiteBuildingStateEvent {
    world::Significance significance{world::Significance::Site};
    SiteXY building;
    BuildingState new_state{BuildingState::Active};
};

static_assert(std::is_same_v<
    decltype(SiteBuildingStateEvent::significance), Significance>);
```

`Region` 級測試先在 tick 1234 建立人口 100，再只呼叫事件入口、不推進 Region 旬回合：

```text
event_immediate significance=Region tick_before=1234 tick_after=1234
before_population=100 after_population=75 region_turns=0
before_hash=651964956482244314 after_hash=17506629468017208808
Debug_ms=0.002215
```

時鐘仍是 1234、旬回合數是 0，但 Region 人口已由 100 變 75；升級路徑 Debug
實測 0.002215 ms，低於 30 ms。把變更前後 Region 各自寫入磁碟世界（root + Region），再走
`world_state_hash` 的磁碟列舉，世界級雜湊確實改變，兩次 `zone_count` 都是 2。

`Site` 級負向控制使用相同建築狀態效果：事件當下 Region 保持 100，只有呼叫該旬歸約後
才變 75。

```text
event_site_negative escalated=0 before_xun=100 after_xun=75
```

## 不算兩次：交集與負向控制

**造得出來，而且兩條路徑確實寫同一個非零人口量。** 測試放兩棟 Active Hall：

1. 基準人口 200。
2. `Region` 級事件把第一棟改 Idle；即時路徑重量完整快照並把人口寫成 175，
   所以事件效果是非零 25。
3. 同一旬內第二棟也改 Idle，但不走升級；正常旬歸約重量兩棟後把同一人口列寫成 150，
   所以待歸約部分也是非零 25。
4. 若第一個事件效果又被扣一次，反事實結果會是 125；實際是 150。

```text
event_no_double_count baseline=200 event_effect=25 after_event=175
other_xun_effect=25 final=150 double_count_counterfactual=125
```

這裡不需要額外「已處理事件」帳本：M2.4 的歸約列本來就是 Site 全量的**絕對快照**，
不是累加 delta。即時路徑與旬末路徑都透過同一張封閉量表量測／覆寫；事件先落到持久來源，
所以旬末快照同時包含事件後狀態與該旬其他變化，既不遺失後者，也不重扣前者。

## M2.3 三次冷往返重跑

```text
site_roundtrip_hashes
18068079511531492422 18068079511531492422
18068079511531492422 18068079511531492422
18068079511531492422 18068079511531492422
building_state=Idle(1)
cold_assertions=3 procedural_disk_empty=1
procedural_recomputed_after_cache_corruption=1
rematerialize Debug max=0.505213 ms
collapse Debug max=0.061956 ms
```

負向控制仍在原斷點斷開：

```text
Idle -> Derelict
collapsed=18068079511531492422
mutated=13476801997497996951
expanded=13476801997497996951
```

## 三個問題

1. **「不算兩次」情境造得出來嗎？** 可以。上面的 `200 → 175 → 150` 同時證明事件
   與旬末歸約都非零地寫人口列，並以 125 作重算反事實負向控制。
2. **沿用重要性等級有沒有卡住？** 沒有。先前程式碼尚無 significance 型別，只有設計
   文件的等級表；這輪把那五級建立成唯一的 `world::Significance`，事件欄位直接使用它，
   沒有事件專用別名或另一套排序。未來實體可直接掛同一型別。
3. **M2 還缺什麼？** 我認為沒有阻止 M2 收線的缺口：投影隔離、持久層、冷往返、固定
   歸約表與即時升級都已有可執行的正／負測試。要透明標出的 surrogate 是：本輪真例是
   「建築狀態離散改變 → 人口／建設快變數」，不是 `城破 → owner`；控制勢力屬任務明令
   不做的其餘八列之一。這不影響界面與重算規則的實證，但未來 owner row 或完整事件系統
   落地時，應再補一條城破驗收，不能拿本輪測試宣稱 owner 語意也已完成。

## 完整驗證

```text
cmake --build build --parallel 2                    PASS（四 target，零警告）
ctest --test-dir build --output-on-failure          156/156 PASS
./build/aetheria_sim --tick 62208000                PASS
godot-mono --headless --path godot --editor ...     exit 0
godot-mono --headless --path godot --quit-after 5   exit 0
CoreIsolation.CompileCommands                       PASS（core 零 godot-cpp）
```
