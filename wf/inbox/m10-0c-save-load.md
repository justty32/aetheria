# 任務書 M10.0c — 遊戲要會存讀檔（波 0 最大的一輪）

**寄件人**：規劃者（調度 session）
**收件人**：gpt-sol 實作者
**必讀**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)、[計畫](../workflows/plan/runtime-injection-waves.md) M10.0c 節
**工作分支**：`m10-0c`（worktree `../aetheria-wt-m10-0c`）

## 背景

可玩 session 用 `InMemoryZoneStore`（`playable_session.h:233`），`bridge/`、`godot/`
零存讀檔呼叫——遊戲根本不會存檔。這輪把它接上，範本是
`tests/zone/diplomacy_save_test.cpp:200-279` 那條冷讀全流程。

## 要做的（7 件，順序建議照列）

1. **用 `ZoneManager` 既有的 root**（建構時已載入或建立，`zone_manager.cpp:40-47`），
   把 `diplomacy_`（`playable_session.h:235`）掛上 root zone 的 `diplomacy` 欄
   （`zone.h:123-124`，只有 root 准掛）。⚠ **不要自己再建第二個 root**。
2. **`region_`／`battle_site_` 收編進 ZoneManager**（現在是 manager 外的散裝成員
   h:236-237，`save_all()` 救不到）。session 保留把手（key／指標），擁有權歸 manager。
3. **部隊態落 registry**：新 component `ArmyState { faction, power, player_controlled }`
   掛上既有部隊 entity，加進 `AllComponents`（`core/serialize/all_components.h`）；
   `PlayableArmy` 退化成執行期把手。**本輪你獨佔 `kSaveFormatVersion`：21 → 22**
   （0a/0b 都不碰格式，不會撞號）。
4. `store_` 改成建構子注入 `zone::ZoneStore&`（既有測試繼續用 InMemory，不必全改）。
5. **存讀檔編排寫成獨立模組**（建議 `core/runtime/session_persistence.{h,cpp}`）：
   吃 `ZoneStore&`＋狀態參考；save＝manager `save_all()`＋`write_manifest`
   （⚠ manifest 要求 root 已在 store，`file_zone_store.cpp:115-117`）。
   **原則九：`PlayableSession` 只呼叫它，不長出序列化邏輯；此模組不 include bridge/godot。**
6. **load 路徑與 new_game 路徑分開**：建構子現在無條件重跑 worldgen＋建軍＋外交初始化
   （`playable_session.cpp:99-170`）；load 必須從 store 重建，再掛回 session 側把手
   （armies 從 registry 的 `ArmyState` 重建、`refresh_quests` 重算）。
   **存檔點語意：只在回合尾端、無 pending 遭遇時准存**（有 `encounter_tile_` 就拒絕並回錯誤）。
7. bridge 加 `save_game(slot_name)`／`load_game(slot_name)`／`list_saves()`
   （存檔根建議 `user://saves/`，由 godot 端傳絕對路徑進 bridge）；
   `godot/main.gd` 加兩顆鈕（存／讀）＋槽名輸入，能看到成功失敗訊息即可，不做美化。

## 驗收（就這 5 條）

| # | 標準 |
|---|---|
| 1 | 新測試：new_game → 推 3 旬（含移動）→ save → **銷毀 session** → load → `get_playable_snapshot` 世界側欄位逐項相等（armies 的 faction/power/位置、tiles、外交）。玩家態 6 成員（residence／observer 座標／accepted_quest／player_army_id）**允許重置為初始值**——那是 M10.2，本輪明列不含 |
| 2 | 冷讀：save 後用**新開的** `FileZoneStore` load（模仿 `diplomacy_save_test` 的作法），世界雜湊與 save 前相等 |
| 3 | `aetheria_sim verify world-hash <slot>` 對存出來的槽通過 |
| 4 | pending 遭遇時 save 被拒且回報清楚錯誤（測試覆蓋） |
| 5 | 全套 `ctest` 綠（`--parallel 2`）；舊 v21 fixture 若有版本比對測試，照 fail-fast 政策更新為 v22（不寫遷移） |

## 模組與依賴（原則九，回報必填）

`session_persistence` 依賴哪些標頭、誰依賴它，畫成兩行清單。

## 不要做的事

| 不要 | 理由 |
|---|---|
| 動 `tests/` 既有雜湊測試檔與 `principle5` 新檔 | 0a/0b 正在並行 |
| 動 `core/serialize/` 的編解碼**邏輯**（`all_components.h` 加一行與必要的 codec 註冊除外） | 格式重地，最小侵入 |
| 存玩家態 6 成員、動 `saves/` 佈局設計（raws/chars） | M10.1／M10.2 的事 |
| 在 godot 端存任何玩法狀態 | 鐵律：顯示層無狀態 |
| build 超過 `-j2`、ctest 超過 `--parallel 2` | 同機另有兩路在編譯 |
| push｜fan-out 子 agent｜改 `design/` | 一律禁止 |

## 回報

`wf/inbox/m10-0c-save-load-complete.md`（≤8KB，繁中）：
第一行標 `DONE`/`BLOCKED`/`NEEDS-USER`/`FAILED`；驗收 5 條逐項證據、
v22 動了格式的哪裡（逐檔列）、模組與依賴節、你被迫做的所有假設。
中途疑問 `.codex-inbox/m10-0c.ask`（**寫下假設繼續做**）；卡超過 10 分鐘必發。

**最重要**：如果你發現「load 後某塊世界態根本無從重建」（不在 zone、不在 manifest、
只活在 session 記憶體），**如實列出那塊是什麼**，不要用重新初始化冒充載入。
這份清單比功能本身更有價值——它就是 M10.1/M10.2 的輸入。
