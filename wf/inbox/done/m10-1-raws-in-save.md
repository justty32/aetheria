# 任務書 M10.1 — 世界檔自帶 raws

**寄件人**：規劃者（調度 session）
**收件人**：gpt-sol 實作者
**必讀**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)、[計畫](../workflows/plan/runtime-injection-waves.md) M10.1 節、
[裁定#8](../workflows/plan/runtime-injection-decisions.md)
**工作分支**：`m10-1`（worktree `../aetheria-wt-m10-1`）
**前置**：M10.0c 已合併進 `main`（本輪從合併後的 `main` 開，**波 1 單線串行**，做完才輪 M10.2）

## 背景

Ruleset 現在一律從全域 `data/` 載：`sim/main.cpp:94`、`bridge/aetheria_core.cpp:332,392,396,442`、
`core/runtime/playable_session.cpp:103,118`（new_game 與 load 兩個建構子都載全域）。後果是**改 `data/` 就弄壞舊存檔**——最硬的證據是
`core/serialize/zone_diplomacy_codec.cpp:57,151`：存檔裡的 `faction_count` 與現行 ruleset 不符即 throw，
所以 `civilization.toml` 多加一個勢力＝所有舊存檔開不了（KNOWN-TRAPS 那條）。
另一個洞：字串 id remap 只檢查「id 還在不在」，同一個 `terrain.swamp` 把 `move_cost` 改掉，
舊存檔照樣載入、行為悄悄變了、沒有任何警告。

裁定#8：存檔的 `raws/` 在世界建立時複製、**之後永不改寫**；載入＝載自己的 raws。
這一輪把兩個洞一起堵掉，並拿到 DF 的行為：**改 `data/` 不會動到舊存檔**。

## 要做的（6 件，順序建議照列）

1. **建新世界時整份複製** `data/` 的 22 個 `*.toml` 到 `<slot>/raws/`（逐檔位元組複製，
   不做差分、不做合併、不重寫任何欄位）。
2. **載入世界＝`RulesetLoader::load(<slot>/raws)`**。槽裡沒有 `raws/` → **fail-fast**，
   訊息講清楚（照專案 fail-fast 政策，**不寫遷移**）。
3. **manifest 加 raws 內容雜湊**（檔名排序後逐檔內容雜湊成一個 64-bit），開槽時比對不符即拒。
   ⚠ 它與 `GenerationParameterHashes` 是**兩個維度並存**：後者只雜湊 12 組
   `RegionGenerationConfig` 欄位（`core/worldgen/region_seed.cpp:30-106`），
   `faction_count` 等 TOML 輸入不在內——**不得互相取代、不得合併進同一個欄位**。
4. **版本 22 → 23，本輪獨佔**。`kSaveFormatVersion`（`core/serialize/zone_codec.h:13`）＋
   三處 `static_assert`（`tests/zone/zone_codec_test.cpp`、`tests/site/site_building_mapping_test.cpp`、
   `tests/site/site_observation_persistence_test.cpp`；行號以合併 0c 後為準）同步。
   **M10.2 不會再 bump，波 1 兩輪共用 v23**（鐵律：每波至多 +1）。
5. **呼叫端一起改**：`sim/main.cpp` 現在**先**無條件用 `--data-dir`／編譯期預設載全域 Ruleset
   （41、57、94 行）**再**開存檔（116 行 `run_world_hash`）——**save 導向的子命令
   （`verify world-hash`）必須改成從 `<slot>/raws` 載**，不吃 `--data-dir`；
   `gen` 系列（不碰存檔）維持 `--data-dir`。`sim/world_hash.h` 的
   `world_state_hash` / `run_world_hash` 目前吃 `const rules::Ruleset&`——**硬性規定**：
   save 導向驗證一律**自己從 `<slot>/raws` 建 Ruleset**，不准再吃外部傳入的
   Ruleset（外部傳入正是「拿錯規則開存檔」這個 bug 的形狀）。
6. **定「exported build 的基準 raws 從哪來」**：`AETHERIA_DEFAULT_DATA_DIR` 是 source tree
   絕對路徑（`cmake/targets_sim.cmake:15-17`、`cmake/targets_bridge.cmake:9-11`），
   裝到別台機就是死路。**這輪定成**：bridge 開新世界時由 godot 端傳入絕對 data 路徑
   （與 0c 的存檔根走同一條路），`AETHERIA_DEFAULT_DATA_DIR` 退成**開發期 fallback**；
   godot export 設定把 `data/` 隨執行檔出貨。若你有更好的做法，先寫 `.ask` 再改。

⚠ **測試 fixture**：`tests/support/ruleset_fixture.h:7-9` 是 process-static 的 Ruleset singleton
（讀 `AETHERIA_SOURCE_DIR "/data"`）——**只准純讀測試用**；raws 相關的新測試**一律每測試私有 raws 目錄**
（複製一份到 tmp 再改），不准動這個 singleton 的語意。

## 驗收（就這 5 條，不要多跑）

| # | 標準 |
|---|---|
| 1 | 新世界建立後 `<slot>/raws/` 有 22 個 TOML，逐檔與 `data/` 位元組相等；manifest 的 raws 雜湊與當場重算相符 |
| 2 | **負向控制（本輪核心，DF 行為）**：建槽 → 改全域 `data/` 三種改動（`terrain.toml` 加一個新地形／改既有 terrain 的 `move_cost`／`civilization.toml` 把 `faction_count` 加一）→ **舊槽照常開，世界雜湊與可觀察行為與改動前逐項相等**。三種改動各附前後輸出 |
| 3 | **負向控制**：手動改 `<slot>/raws/` 任一 TOML 一個 byte → 開槽被拒，訊息指出 raws 雜湊不符；還原後綠。附兩次輸出 |
| 4 | 第 2 條的 `data/` 改動之後**新開**一個世界 → 新槽吃到新 `data/`（新地形可用、`faction_count` 是新值），且與舊槽並存、互不影響 |
| 5 | `aetheria_sim verify world-hash <slot>` 對新舊兩槽都通過（各讀自己的 raws）；全套 `ctest` 綠（`--parallel 2`），總數不減 |

## 模組與依賴（原則九，回報必填）

raws 的複製／雜湊／路徑解析寫成**獨立模組**（建議 `core/runtime/save_raws.{h,cpp}`）：
吃路徑、吐雜湊與 raws 目錄；**不 include `PlayableSession`／`bridge/`／`godot/`**，
`PlayableSession` 只呼叫它。回報畫兩行清單：它依賴誰、誰依賴它。

## 不要做的事

| 不要 | 理由 |
|---|---|
| 動 Ruleset 的可變性（追加、`RulesetPatcher`、注入） | M10.3b 的事 |
| 做 raws 差分／合併，或替沒有 `raws/` 的舊槽寫遷移 | 裁定#8 就是整份複製；舊槽 fail-fast |
| 存玩家態 6 成員、建 `chars/` 佈局 | M10.2（同一條線的下一輪） |
| 再 bump 一次版本，或動 v22 既有欄位的語意 | 波 1 只准 +1，你獨佔 23 |
| 把 raws 雜湊塞進 `GenerationParameterHashes` | 兩個維度，明文並存 |
| 改 `tests/support/ruleset_fixture.h` 的 singleton 語意 | 只准純讀測試用 |
| build 超過 `-j2`、ctest 超過 `--parallel 2` | 同機可能另有一路在編譯 |
| push｜fan-out 子 agent｜改 `design/` | 一律禁止 |

## 回報

`wf/inbox/m10-1-raws-in-save-complete.md`（≤8KB，繁中）：
第一行標 `DONE`/`BLOCKED`/`NEEDS-USER`/`FAILED`；驗收 5 條逐項證據（兩個負向控制的前後輸出必附）、
v23 動了格式的哪裡（逐檔列）、第 5 件與第 6 件的最終做法與理由、模組與依賴節、你被迫做的所有假設。
中途疑問寫 `.codex-inbox/m10-1.ask`（**寫下假設繼續做**）；卡超過 10 分鐘必發。

**最重要**：如果你發現有程式路徑**無法**從存檔自己的 raws 拿到 Ruleset——例如某個 static／singleton
快取了全域 `data/` 的 def 指標或整數下標、或某層拿 `AETHERIA_DEFAULT_DATA_DIR` 當事實來源——
**如實列出來**，不要用「反正兩邊內容一樣」蒙混過去。那條路就是 M10.3b 注入時會炸的地方，
這份清單比功能本身更有價值。

（行號已對 M10-INT-0 併入後的 main 核校：v22 定義與三處 static_assert 在
`zone_codec_test.cpp:103`、`site_building_mapping_test.cpp:149`、
`site_observation_persistence_test.cpp:110`。）
