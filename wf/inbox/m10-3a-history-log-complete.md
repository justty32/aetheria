DONE
# M10.3a 歷史日誌、結算恢復與重放已完成，429 項測試全綠

## 實作結論

- 新增 payload 無關的 append-only `HistoryLog`：條目含 `seq/tick/kind/payload/prev_hash/entry_hash`，以字串 kind、TOML 原文 payload 與 FNV-1a 鏈落在 `history.log`；讀檔及 `verify()` 都逐筆重算。
- 新增世界級 `TurnCommit`：玩家命令先寫日誌、保留舊 committed seq 的獨立 `history.commit`，套用成功並以既有 `save_session` 寫完 zone/manifest 後才用 tmp+rename 前推 marker；開槽發現尾巴便自動重放。
- `issue_move`、`advance_xun`、`resolve_encounter` 與 bridge dispatcher 對應的全部 18 個覆蓋字串入口，都在 core session 方法首行經同一 `record_command` 記錄；未新增 enum。
- `sim replay <slot>` 在全域 ruleset 載入前分派，以槽內基底 raws、seed 及完整日誌建立新世界並比較世界身分。
- manifest 既有兩個 next-id 欄位正式成為配發器；新世界初始偏好 UID 移至基底 raws，core 不再含三個固定數字。

## 六條驗收證據

### 1. 現役、冷讀、完整重放世界身分相等

命令含 move、兩旬、戰鬥，以及 enter/build/leave 三個覆蓋命令：

```text
stored_world_hash=17057310942711789320 replay_world_hash=17057310942711789320 history_seq=7
acceptance1 active_hash=17057310942711789320 cold_hash=17057310942711789320 replay_hash=17057310942711789320
```

### 2. 中段 byte 篡改：先紅、還原後綠

```text
acceptance2 tampered=RED error=history 雜湊鏈在第 2 筆斷裂：entry_hash 不符
acceptance2 restored=GREEN history_seq=7
```

### 3. 日誌與舊 marker 已落、套用前中止：先紅、重啟恢復後綠

```text
acceptance3 interrupted=RED error=M10.3a 測試鉤：日誌與 marker 已落，套用未跑
acceptance3 recovered=GREEN recovered_hash=14121863355086391575 direct_hash=14121863355086391575
```

### 4. v23 零命令基準負向控制：注入命令先紅、移除後逐位元綠

```text
acceptance4 command_injected=RED v23_hash=17114528469974780418 actual_hash=12621902662656090292
acceptance4 command_restored=GREEN v23_hash=17114528469974780418 actual_hash=17114528469974780418
```

沒有為門檻反覆試值；測試以固定 seed 一次比較既有 v23 計算法與新公式。

### 5. UID 無硬編碼且冷讀續配不撞號

```text
$ rg -n '1001|2001|9001' core
（無輸出）
acceptance5 uid_sequence=1001,2001,2002,2003
```

前兩個是 raws 指定的初始偏好值，後兩個分別由第一次及第二次冷讀的 manifest 配發器取得。

### 6. 冷讀前後戰鬥逐欄位相等；全套測試不減

```text
acceptance6 uninterrupted={resolution=15500165044611146076,expected=7092/26943,loss=7517/26518,contribution=26518/0/0/0/26518/0/26518} cold={resolution=15500165044611146076,expected=7092/26943,loss=7517/26518}
$ ctest --test-dir build --output-on-failure --parallel 2
100% tests passed out of 429
Total Test time (real) = 54.02 sec
```

測試以完整 `layer_fields(...)` tuple 做 `EXPECT_EQ`，包含輸出列中的 resolution、雙方 expected/loss 與 contribution 七欄。總數由 423 增為 429，正好新增六條 `HistoryAcceptance`，沒有減少。完整建置亦以 `cmake --build build -j2` 通過。

## 存檔格式異動清單（供整合輪定 v24）

- `core/history/history_log.{h,cpp}`：新增獨立二進位 `history.log`，含 magic、固定寬度欄位、字串長度與內容；是新格式面，但未進 zone codec/manifest。
- `core/runtime/turn_commit.{h,cpp}`：新增獨立 `history.commit`，內容恰為 little-endian u64，tmp+rename 原子替換；未塞入 manifest。
- `data/civilization.toml`：新增 `[playable_session_uids]` 三個基底 raw 值，因此新世界 raws 內容與 raws hash 會反映此節。
- `core/zone/zone_store.{h,cpp}`、`file_zone_store.{h,cpp}`、`core/runtime/session_persistence.cpp`：啟用 v23 manifest 已有的 `next_detached_zone_id`／`next_entity_uid` 並在存檔複製目前計數；欄位、編碼順序與檔案版型均未新增。
- `sim/world_hash.{h,cpp}`：世界身分語意納入 raws 與日誌鏈頭；不改任何磁碟 codec。

`core/serialize/zone_codec.h` 仍為 `kSaveFormatVersion = 23`；未改 `AllComponents`、zone/manifest/角色 codec 版型。

## 亂數最終形狀

目前戰鬥 `event_id = 當前玩家命令的 history seq`，亂數種子為：

```text
mix(seed XOR rotl(uint64(tick), 21) XOR rotl(history_seq, 43))
```

`mix` 使用兩輪固定 64-bit avalanche 常數。`revision_` 僅保留 UI 髒標記用途，`next_event_id_` 已移除，兩者均不再進亂數輸入；named-fate 的 event id 同樣取持久化 seq。

世界身分實際正規化為 `zone_hash XOR raws_hash XOR (head_hash XOR genesis_hash) XOR v23_base_raws_salt`；其中 genesis 就是該槽 raws hash。固定 salt 僅校準 v23 零命令既有值，日誌增量與非基準 raws 都會改變身分。

## 模組與依賴（原則九）

- `history_log`：依賴 `core/base/check.h`、`core/time/tick.h`、標準庫；被 `turn_commit`、`sim/world_hash` 與驗收測試依賴，沒有 include 其他 core 子目錄或 bridge。
- `turn_commit`：依賴 `history_log`、zone store/manager、`session_persistence`，並負責解讀 `PlayableSession` payload；被 `PlayableSession` 及重放驗收間接依賴，bridge 不持有日誌狀態。

## 被迫假設

1. 任務書的「marker 已落」解讀為套用前確保獨立 marker 存在但仍寫「最後已完整落盤」的舊 seq；若先寫新 seq，崩潰後反而無法判斷尾巴。已寫 `.codex-inbox/m10-3a.ask` 後照此繼續。
2. 零命令既要逐位元相容 v23、又要讓 raws/日誌進身分，因此以 raws 作鏈 genesis，日誌項使用相對 genesis 的增量，再加固定 v23 基準 raws salt；非基準 raws 仍可觀測。
3. 為保留既有 v23 初始世界結果，1001/2001/9001 移到 `civilization.toml` 當 allocator 的 preferred 值，而非任意改初始 ID。
4. 任務書文字稱 16 個覆蓋命令，但目前 header/bridge dispatcher 實際有 18 個字串入口；採實際完整集合記錄，未刪減舊入口。
5. 崩潰鉤依驗收明定位置放在 append＋舊 marker 落盤後、任何命令套用前。

## 無法只靠 seed＋基底 raws＋日誌重建的清單

1. **多角色槽的角色選取／匯入生命週期**尚不是玩家日誌條目。實測既有雙角色槽完整重放得到 `stored=10773397634216242476`、`replay=10707280563954706049`（history seq 10）；即使每筆 payload 帶 residence/座標/玩家部隊/任務 context，角色 save 的選取與 zone materialization 軌跡仍無法由目前三項輸入位元級重建。`sim replay` 本輪只證明任務要求的單角色命令線。
2. **被 session 拒絕的玩家命令**依 journal-first 原則仍會先留下輸入，但日誌沒有「拒絕結果／未套用」條目；若狀態脈絡不足，重放會在該筆再次丟例外，無法自行判定應略過或視為有效輸入。

單角色六條驗收路徑未發現 filesystem 走訪順序或未持久化亂數計數器依賴。上述缺口未以「跑起來相似」掩蓋，留給後續設計裁定。

## 其餘守門

- `python3 tools/check_principle5.py`：`principle5 check passed: 54 populated enums (45 mechanisms, 9 debt)`。
- `git diff --check` 通過；沒有 `design/` 變更。
- 未碰 RulesetPatcher、def 套用、`add_faction`、外交 codec、`inject/` 入口、SQLite、截斷／改寫日誌、named fate 持久化；未 push。
