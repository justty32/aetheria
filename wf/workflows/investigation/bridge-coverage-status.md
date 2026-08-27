# bridge 覆蓋率盤點 — core 做的東西有多少從 bridge 露出去

← [investigation](README.md)｜2026-08-27｜基準 `main`（未含 `../aetheria-wt-m10-0*`）

## 問題

**「core 做了的東西，有多少從 bridge 露出去、godot 拿得到？」**
M8 曾量到「M6/M7 做的戰鬥、勢力 AI、外交、地城、湧現任務，從遊戲裡一個都碰不到」
（[m8-2-loop-coverage.md](../../inbox/m8-2-loop-coverage.md) 的 6 列 ❌）。之後還剩多少？
拆成 5 個可判定提問，逐一對應下面〈現況〉的 5 節。

## 結論

**M8.2 接通的是動作，不是資料。** bridge 對 `PlayableSession` 幾乎全開（37 個方法露 36 個），
但它只是 M8.1/M8.2 的示範門面——**缺口從 bridge↔session 移到了 session↔core**。
九子系統：**完全沒露 1、部分露 7、有露 1、完整露 0**；Lua/主線劇情另計，也完全沒露。
M8 那 6 列 ❌ 現在是「摸得到入口、看不到內容」。

## 現況

### 1. bridge 的全部表面

**9 個 bound method**（`aetheria_core.cpp:264-291`，宣告 `aetheria_core.h:25-51`）：
`get_core_version()`、`tick_to_date(tick)`、`generate_region(seed,region_id)`、`poll_events()`、
`new_game(seed,region_id)`、`get_playable_snapshot()`、`issue_move(unit_id,x,y)`、`advance_xun()`、
`resolve_encounter(choice)`、`coverage_command(command)`。

`get_playable_snapshot` 吐 **37 個頂層欄位**（`:403-476`）：
width, height, base, relief, feature, temperature, moisture, elevation, edges, owner,
settlement, population, order, tile_blob, terrain_ids, relief_ids, feature_ids, edge_ids,
portals, units, events, encounter_pending, revision, tick, player_unit_id, guided_target_x/y,
battle_tile_x/y, battle_report, coverage, site_view, local_view, coverage_tile_x/y,
batch_bytes, batch_ms。

子 Dictionary：`coverage` **22 欄**（`:228-259`）、`battle_report` **16 欄**（`:184-209`）、
`site_view`/`local_view` 各 6 欄（`:213-224`）、`units` 6 欄、`events` 5 欄；`coverage_command`
收 **18 個命令字串**（`:564-599`）。⚠ `PlayableCoverageSummary` 24 個成員只打包 22：
`persistent_buildings`/`persistent_population`（`playable_session.h:134-135`）無欄位。

### 2. 九個子系統

「沒露」皆 grep 過 `bridge/` 三檔：`Diplomacy` `declare_war` `faction_ai` `EmergentQuest`
`NamedFate` `fov` `Formation` `magic` `faith` `Race` `script` `lua` `save` **全為 0 次**。

| 子系統 | 判定 | core 有的 → bridge 露的 |
|---|---|---|
| 外交 | 部分露 | `diplomacy.h:99-165` **25 個公開方法**（`declare_war`:130、`observe_faction`:159）→ 只 `sign_treaty`＋`treaty_count`(`:242`)；戰爭、關係、情報零欄位 |
| 勢力 AI | 部分露 | 真有跑(`playable_session.cpp:882`)、`FactionActionKind` 7 種 → 只把 kind 壓成整數塞進 `value_b`(`:885-889`)；理由/目標無欄位，參數寫死 |
| 地城 | 部分露 | `dungeon.h` 14 函式（生成`:143`、機關`:156,160,163`、寶藏`:179`）→ 4 命令＋3 純量(`:239-241`)；層數、機關、寶藏內容全無 |
| 湧現任務 | 部分露 | `EmergentQuestKind` **5 種**皆有偵測(`emergent_quest.cpp:51,66,81`)、僅 2 種有 `complete_*` → 只 2 種有旗標(`playable_session.cpp:1034,1039`)、1 種可動作，無文字 |
| named fate | 部分露 | `named_fate.h` 10 型別＋`FateResolver` 4 個 static(`:171-178`) → 戰報只 `named_person`/`named_outcome`(`:200-201`)；帳本是區域變數、用完即丟(`:953`) |
| 三層駐留 | **有露** | `PlayableResidence` 四層 → `coverage.residence`(`:230`)＋18 命令，`main.gd:6,109` 依它切 UI |
| Local 探索 | 部分露 | `calculate_fov`(`local_fov.h:36`)、`assess_exploration_step`(`local_movement.h:30`) → 只有 `local_view` 的 `cells`＋`player_x/y`(`:213-224`)；**FOV 無欄位**，尋路不可呼叫 |
| 戰鬥細節 | 部分露 | `CohortFormation{Line,SpearWall,Loose}`、`ContactArc`(`site_combat.h:17-94`)、`LocalCombatant`(`local_combat.h:70`) → `battle_report` 16 欄全是聚合純量(`:204-208`)；cohort／朝向／接敵面／個體全無 |
| 魔法/信仰/種族 | **完全沒露** | `power_sources.h:19-159` 9 型別＋`Ruleset::races()`(`ruleset.h:143`) → **0 個欄位**。⚠ 且 `resolve_power_profile`、`fall_deity` 等在 `core/`、`bridge/`、`godot/` **0 個呼叫端**，只有 `tests/rules/power_sources_test.cpp` 用——**整塊在 core 內就是孤兒** |

### 3. PlayableSession：37 個公開方法，bridge 呼叫 36 個

`playable_session.h:148-204` 共 **37 個**公開方法（不含建構子），bridge 呼叫 **36 個**。唯一沒被
呼叫的是 **`tile_state`**（`:156`），只被 `playable_session.cpp:933,987` 內部用來填戰報。
**沒有「只有測試在用」的公開方法**——這層覆蓋率 36/37。

### 4. 事件：20 種全發、20 種顯示、2 種被解讀

`PlayableEventKind` **20 種**（`playable_session.h:32-53`），core 實際 emit **20 種全部**；
`godot/main.gd:7-13` 的 `EVENT_NAMES` 也剛好 20 條。但 `_format_events`（`main.gd:396-407`）
只對 **2 種**解讀 payload：`kind==3`（FactionAiActed）與 `kind==5`（Encounter），其餘 18 種
只印 `數字=a→b`。

⚠ **`poll_events()` 與這條路無關**：它讀 bridge 自己的 `make_fate_presentation_fixture()`
（`aetheria_core.h:54`），不是 session 事件；唯一消費者 `event_panel.gd:13` 所在的
`event_panel.tscn` **未被 `main.tscn` 載入**——9 個方法有 1 個接在孤兒場景上。

### 5. Lua/script 與主線劇情：沒露，且可玩流程裡根本沒跑

- M8.0 完成回報自己就寫明「沒有修改 `bridge/`、`godot/`」。更關鍵：
  `RegionTurnPipeline::advance_xun` 第 4 參數 `ScriptTurnPass` 預設為空
  （`region_movement.h:112,126`），而 `PlayableSession::advance_xun` **只傳 3 個 callback**
  （`playable_session.cpp:867-891`）——**可玩 session 從不執行任何腳本**。`ScriptEngine` 在
  `core/`、`bridge/`、`godot/`、`sim/` **零呼叫端**，只有 `tests/script/lua_sandbox_test.cpp` 用。
- **M9.0 主線劇情：文件說了、程式碼一行都沒有。** 任務書要的「擴充 `Context`」沒發生——
  `script/context.h:33-35` 只有 `owner`/`set_owner`/`rng()`。全 repo **`.lua` 檔 0 個**、無
  `QuestDef`；`mainline`/`storyline` 只命中 `VictoryCheck` 骨架（`script_engine.h:21,26,78`），
  而它只有測試呼叫。任務書仍在 inbox、不在 `done/`。⚠ **別被版本號誤導**：
  `kSaveFormatVersion` 雖已 20→**21**（`zone_codec.h:13`），但 `git log -S` 查到那次 bump 來自
  **`e786465`（M8.3 建築型別映射）**，不是 M9.0。**主線劇情不被任何東西驅動。**

## 差距

1. **魔法/信仰/種族位階**：core 9 型別 → bridge 0 欄位，core 內部也 0 呼叫端。
2. **Lua 不在可玩流程中執行**：`ScriptTurnPass` 沒被傳，掛勾點是死的。
3. **M9.0 主線劇情未實作**：`Context` 未擴充、`.lua` 0 個、無 `QuestDef`。
4. **外交**只露條約；**勢力 AI** 只剩 kind 整數且參數寫死；**戰鬥**只露聚合純量；
   **Local** FOV 與尋路不可達；**湧現任務** 5 種只 1 種可動作。
5. 零碎：`persistent_*` 兩欄沒打包；`poll_events()` 接在未載入的場景上；**無存讀檔方法露出**。

**缺口最大排名**：① 魔法/信仰/種族位階（唯一「bridge 0 欄位 **且** core 0 呼叫端」的整塊）；
② Lua／主線劇情（掛勾層蓋好了但從不執行、無腳本資產）；③ 外交（25 個方法只露 1 個）。

## 牽扯到的部份

- `bridge/aetheria_core.{h,cpp}`（9 個 bound method、`_bind_methods()`、5 個 `pack_*`）
- `core/runtime/playable_session.{h,cpp}`（門面；`advance_xun` 的 callback 數）
- `godot/main.gd`、`event_panel.gd`、未載入的 `event_panel.tscn`、`main.tscn`
- `core/serialize/zone_codec.h:13` 的 `kSaveFormatVersion`（21，來自 M8.3）與存檔測試
- [KNOWN-TRAPS.md](../../KNOWN-TRAPS.md)「不會存檔」「named fate 持久化斷裂」
  「原則七插入點是死代碼」三條同源；`../aetheria-wt-m10-0*` 三個 worktree 未涵蓋
