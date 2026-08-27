# 任務書 M10.2 — 角色檔

**寄件人**：規劃者（調度 session）
**收件人**：gpt-sol 實作者
**必讀**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)、[計畫](../workflows/plan/runtime-injection-waves.md) M10.2 節、
[裁定#6](../workflows/plan/runtime-injection-decisions.md)（同一世界可掛多個角色檔）
**工作分支**：`m10-1`（**同一個 worktree** `../aetheria-wt-m10-1`，接在 M10.1 的
commit `80f0c21` 之後——波 1 單線串行，你是波 1 第二輪也是最後一輪，整波一起併 main）

## 背景

0c 之後世界態進得了存檔，但玩家態 6 成員仍只活在 session 記憶體，load 後被重置為初始值
（0c 驗收第 1 條明列「不含」）。使用者裁定：**世界是一個存檔、人物是一個存檔**，
同一個世界可以掛多個角色檔。這一輪把玩家態拆出去。

現況 6 成員（`core/runtime/playable_session.h`，行號已對本分支現況核校）：
`player_army_id_`(266)、`residence_`(275)、`local_z_`(276)、`local_player_x_`(277)、
`local_player_y_`(278)、`accepted_quest_id_`(281)；`local_door_open_` 在 279。

## 要做的（7 件，順序建議照列）

1. **`saves/<世界>/chars/<角色>.bin`**，小 codec（cereal PortableBinary，與 zone codec 同一套
   序列化庫但**獨立檔案、獨立入口**）。
2. **獨立的 `kCharacterFormatVersion`**（從 1 起），放在角色 codec 自己的標頭，
   **絕不共用 `kSaveFormatVersion`**——否則世界每 bump 一次（v24、v25…）角色檔就無條件失效。
   版本不符 → fail-fast，訊息清楚。
3. **存那 6 個成員**，一律字串 id／`StableId`，**不存型別整數下標、不存指標、不存 `const Def*`**。
4. **角色檔要認得它屬於哪個世界**（裁定#4 已定）：存 `world_seed` **＋** manifest 的
   `raws_hash` 兩個欄位，載入時與該槽 manifest 比對，**任一不符即拒**（fail-fast；
   基底 raws 不可變，raws_hash 就是世界身分的一半）。
5. **`local_door_open_`(279) 搬進 Local zone**——它是**世界態**，不是玩家態。
   最窄的落法：Local zone 的 registry 開一個 component 記已開的門
   （`local::LocalEdgeAddress` 集合），`door_query`（`core/runtime/playable_session.cpp:91`）
   改成讀它。**不要順手做整套門系統**（開關動畫／鎖／鑰匙一律不做）。
6. **world-hash 掃描要跳過 `chars/`**：`sim/world_hash.cpp` 的 `find_zone_files`(44-83) 目前
   只排除 `manifest.bin`，其餘每個 `.bin` 都遞迴撿起來丟給 `key_from_path`(31-41) 當
   16 碼 ZoneKey 解——**第一個角色檔就會讓它 throw**。改成只認 zone 檔
   （`root.bin` ＋ 256 桶目錄下的 16 碼檔名），或明文排除 `chars/`。
7. **bridge**：`save_game` / `load_game` 帶**角色名參數**，加 `list_characters(slot)`；
   `godot/main.gd` 加角色名輸入欄，能看到成功失敗訊息即可，不做美化。

**版本協調**：波 1 只准 +1，M10.1 已把 `kSaveFormatVersion` 推到 **23**；
**本輪不再 bump**，第 5 件的 Local zone 門狀態一起吃 v23。
若你判斷非 bump 不可，**先寫 `.ask` 問，不要自己動**。

## 驗收（就這 5 條，不要多跑）

| # | 標準 |
|---|---|
| 1 | 兩角色互不污染：建世界 → 角色 A 玩若干旬（移動、進 Local、開門、接任務）→ 存 → 角色 B 從初始態玩另一路 → 存 → 重載 A，A 的 6 成員逐項等於它存檔當下的值 |
| 2 | 往返一致：存 → **銷毀 session** → 冷載入，6 成員逐項相等；同一狀態存兩次，角色檔位元組相同 |
| 3 | **負向控制**：刪掉 `<slot>/chars/A.bin` → 世界檔照常開、角色 B 照常載入；把 A 的角色檔拿去開另一個 `world_seed` 不同的世界 → **被拒**，附錯誤訊息 |
| 4 | **負向控制**：`aetheria_sim verify world-hash <slot>` 在 chars/ 有 0／1／2 個角色檔三種狀態下都通過，且**三次世界雜湊相同**（角色檔不進世界身分）。附三次輸出 |
| 5 | `local_door_open_` 移位後：開門 → 存 → 冷載入，門**仍是開的**，且此事實對 A、B 兩個角色皆可見；全套 `ctest` 綠（`--parallel 2`），總數不減 |

## 模組與依賴（原則九，回報必填）

角色檔 codec 寫成**獨立模組**（建議 `core/runtime/character_save.{h,cpp}`）：
吃一個 POD 的角色狀態結構＋路徑，**不 include `PlayableSession`、不 include bridge/godot**；
`PlayableSession` 只提供 export／import 兩個窄函式把 6 成員搬進搬出。
回報畫兩行清單：它依賴誰、誰依賴它。**一行序列化邏輯都不准長進 `PlayableSession`。**

## 不要做的事

| 不要 | 理由 |
|---|---|
| 存 `quests_` | 衍生快取，載入後 `refresh_quests` 重算 |
| 存量測／校準成員（`dungeon_density_*`、`last_order_*`、`roundtrip_hashes_`、`calibration_*`） | 不是玩家態，也不是世界態 |
| 動 `kSaveFormatVersion` 或世界 zone codec 邏輯（第 5 件必要的最小改動除外） | 波 1 已用掉 +1 |
| 動 `<slot>/raws/` 佈局與 M10.1 的 raws 雜湊 | M10.1 的領地，已合併定案 |
| 做多角色同時在場、角色間互動、角色刪除 UI | 不在本輪範圍 |
| 把門做成完整門系統（鎖、鑰匙、動畫） | 只搬存放位置，不擴功能 |
| 在 godot 端存任何玩法狀態 | 鐵律：顯示層無狀態 |
| build 超過 `-j2`、ctest 超過 `--parallel 2` | 同機可能另有一路在編譯 |
| push｜fan-out 子 agent｜改 `design/` | 一律禁止 |

## 回報

`wf/inbox/m10-2-character-file-complete.md`（≤8KB，繁中）：
第一行標 `DONE`/`BLOCKED`/`NEEDS-USER`/`FAILED`；驗收 5 條逐項證據（兩個負向控制輸出必附）、
`kCharacterFormatVersion` 存了哪些欄位（逐欄列）、`local_door_open_` 最後落在哪個 component
與 `door_query` 怎麼改、world-hash 掃描改法、模組與依賴節、你被迫做的所有假設。
中途疑問寫 `.codex-inbox/m10-2.ask`（**寫下假設繼續做**）；卡超過 10 分鐘必發。

**最重要**：如果 6 成員裡**還有哪一個其實不是玩家態**（像 `local_door_open_` 那樣是世界態偽裝的），
或哪一個**離開 session 就無法重建**（例如 `player_army_id_` 指的 army 在載入後的 registry 裡找不到），
**如實列出來並說清楚你怎麼處理**——不要為了讓往返測試變綠，就把世界態塞進角色檔。
這條界線劃錯，M10.4 之後每一輪注入都會踩到。

---

## 附錄：兩個已知事實

- `find_zone_files` 只撿 `.bin`，M10.1 的 `raws/`（`.toml`）不受影響——**只有 `chars/*.bin` 會炸**它。
- 門狀態現況沒有持久結構（`door_query` 回傳常數 bool，`playable_session.cpp` 一帶）：
  第 5 件**不是搬移，是新建最小持久門狀態**——範圍已由調度者裁定為上面寫死的最窄形狀，
  不要再長。
