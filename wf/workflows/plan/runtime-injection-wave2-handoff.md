# 波 2 交接 — 暫停於 M10.3a（2026-08-27）

← [總覽](runtime-injection.md)｜[波 2 細目](runtime-injection-inject.md)｜[裁定](runtime-injection-decisions.md)｜[TEAM](../../TEAM.md)

> **給下一任波管家冷啟動用。** 波 2 因 token 配額暫停，**M10.3a 已驗收通過並併入 `main`**，
> 3b 與 M10.4 尚未開工。讀完本檔即可接手，不必回頭讀三輪的回報信。

## 現況

| 輪 | 狀態 |
|---|---|
| **M10.3a** 歷史日誌＋結算協調器 | ✅ **已併入 main**（初版＋fix1＋fix2），432/432 綠，**未 bump，仍 v23** |
| M10.3b def 注入（`RulesetPatcher`） | ⬜ 未開工，**任務書尚未起草**（codex 起草、管家定稿） |
| M10.4 勢力可增長 | ⬜ 未開工，任務書尚未起草 |
| 波 2 定版輪 | ⬜ 未開工。v23→24、三處 `static_assert`、彙總格式變更、全套綠 |

## v24 定版輪要用的格式變更清單（原文保留，勿改寫）

M10.3a **零二進位佈局變更**，所以不 bump；但下列是**語意面**的變更，定版輪要一起交代：

- `history.log`：**新增**的獨立二進位檔（magic、固定寬度欄位、字串長度＋內容）。
  鏈語意＝第一筆 `prev_hash = 0`（`kHistoryGenesisHash`），**不由 raws 播種**；
  ⚠ 以 raws 當 genesis 寫出的舊鏈與現行不相容。
- `history.commit`：**新增**的獨立 little-endian u64（marker）。**沒有塞進 manifest**。
  交易語意＝marker **最後**寫；**缺檔代表未提交**；舊值代表需由完整日誌重建。
- `chars/<角色>.bin`：codec／版本**未改**，只把原子寫入移到 marker **之前**。
- `data/civilization.toml`：新增 `[playable_session_uids]`（player_army 1001／enemy_army 2001／
  named_commander 9001）。三個在建世界時**一次全 claim**，故 `next_entity_uid` 初始推進到 9002。
- `core/zone/zone_store`／`file_zone_store`／`session_persistence`：啟用 v23 manifest **已有的**
  `next_detached_zone_id`／`next_entity_uid`（原本是死欄位）。**欄位、編碼順序、版型均未新增或重排。**
- `sim/world_hash.{h,cpp}`：身分語意改為三分量定序 FNV-1a、`verify world-hash` 輸出改四行；
  `cmake/check_sim_world_hash.cmake` 只加斷言。**兩者都不屬磁碟 codec。**
- `core/serialize/zone_codec.h` 仍為 `kSaveFormatVersion = 23`。

## 綁定裁定（已生效，別再重開）

- **世界身分雜湊 ＝ zone 正規化雜湊 → raws 雜湊 → 日誌鏈頭的定序 FNV-1a 鏈合成。**
  ⚠ **不准用 XOR**（可交換、允許抵消）；**空鏈頭＝固定常數 0，永不從世界資料播種**；
  **不准引入任何「校準／相容」常數**。計畫檔支柱 3 已同步修訂。
- commit marker ＝「**已套用的最後 seq**」；順序＝日誌落盤（marker 持舊值）→ 套用 →
  zone/manifest／角色檔落盤 → **成功後**才原子前推。其餘沿裁定 #2／#4／#7／#8／#9 不變。

## 負向控制：誰的手跑的

**管家（Claude）親手跑的**——以下每一條都是自己注入、自己看紅、自己還原看綠，**不是重跑 codex 的測試**：

1. 用 Python **獨立重算**定序 FNV fold，兩組真實數字（head=0 與 head≠0）都精確命中 binary 的
   `world_hash`；交換 zone/raws 順序得不同值（不可交換）；拿掉 raws 得不同值（raws 真有貢獻）。
2. 篡改 `raws/` → fail-fast（manifest raws_hash 不符）；翻 zone `.bin` 一 byte → fail-fast。
3. 篡改 `history.log` **中段** byte → fail-fast 且**點名「第 4 筆」**；還原後四個數字逐位元回原值。
4. **崩潰恢復**（本波最重要）：marker 7→綠、降到 4→**修前紅**（`示範住宅已經蓋過`＝重複套用）、
   修後→綠**且雜湊與基線逐位元相同**；marker 檔刪掉→也能恢復。
   **判別控制**：刪一個 zone 分片，marker=4 **救得回來**（重建到基線雜湊）、
   marker=7 **救不回來**（`存檔缺少必要 zone：戰鬥 Site`）——證明恢復真在跑且由 marker 把關。
   另查載入後 marker 由 4 **被前推到 7**，證明恢復有提交。
5. `check_sim_world_hash.cmake` 的控制打掉（`move_cost` 9 改回 3）→ **紅**；還原 → 綠。
6. 全套 `ctest --parallel 2` → **432/432 綠**。

**雜務手（sonnet，數字經管家逐個核對）**：全套 ctest 一次；用 `main` 的程式碼在拋棄式 worktree
**獨立算出 v23 zone 基準值 `17114528469974780418`**，與測試寫死的 `kV23ZoneHash` 相等（兩種算法各一次）。

## ⚠ 未解／會咬人的

**A. 無法只靠 seed＋基底 raws＋日誌重建（codex 如實回報，3a 最有價值的產出）**

1. **多角色槽的角色選取／匯入生命週期尚不是日誌條目。** 實測雙角色槽完整重放
   `stored=10773397634216242476`、`replay=10707280563954706049`（history seq 10）——**對不上**。
   即使 payload 帶 residence／座標／部隊／任務 context，角色 save 的選取與 zone materialization
   軌跡仍無法位元級重建。`sim replay` 目前只證明**單角色**命令線。⚠ **3b 與 M10.4 會炸在這。**
2. **被 session 拒絕的玩家命令**依 journal-first 仍會先留下輸入，但日誌沒有「拒絕／未套用」條目；
   脈絡不足時重放會在該筆再次丟例外，無法自行判定該略過還是視為有效輸入。

**B. 仍無法自動恢復的崩潰點（fix2 自行回報）**

1. 首次建槽崩在**完整 manifest 出現以前**：磁碟無可稽核的 seed／region 基線，`FileZoneStore`
   fail-fast。要涵蓋需另立建槽交易／基線 metadata——**不能偷塞 manifest 欄位**（會逼出 bump）。
2. `history.log` 單筆 append 被 kill 成**截斷尾筆**：整鏈驗證 fail-fast。append-only 且禁止
   截斷重放的約束下，不能把破尾當成已完成輸入。

**C. 繼承缺陷（管家發現，已在 fix2 修掉）**

`cmake/check_sim_world_hash.cmake` 從 v23 起就**擺好整套負向控制卻從不比較兩槽輸出**，
只用 regex 斷言「長得像數字」——把 terrain 改回去它一樣綠。已改成實際比較四分量並斷言。
⚠ **值得順手掃其他 `cmake/check_*.cmake`**：同一種病可能不只一處。

**D. 值得記住的事實**：改 `terrain.swamp` 的 `move_cost`，**`zone_hash` 完全不動**
（兩槽都是 `4698212455085739946`），只有 `raws_hash` 變。**raws 分量抓得到 zone 分量瞎掉的東西**
——計畫第 6 點與「連根拔掉 XOR」那個裁定的活證據。

## 派工心得（這一波踩到的）

- ⚠ **codex 會漏讀 `.codex-inbox/<輪>.reply`。** 初版輪它 11:08 寫 `.ask`、管家 11:12 覆寫
  `.reply` 放進裁定，它 11:41 完成卻仍照**被否決的**假設做完。**對策**：prompt 第一行明令先讀
  `.reply`，並要求**做完前回頭再掃一次**——fix1／fix2 加了之後兩輪都回報「完成前已再次讀取」。
- ⚠ **「codex 死了」的誤判**：`-o` 檔只在**結束時**才寫、`| tail -N` 緩衝到 EOF、commit 只在最後——
  三個「異常」全是正常。一律 `pgrep -x codex` 後讀 `/proc/<pid>/cmdline` 比對 `-C`（OPS-NOTES 記過）。
  順帶：`grep -l <字串> /proc/*/cmdline` 因 NUL 分隔**會回空**，不能當「不存在」的證據。
- **驗收要自己造槽**：`sim` 沒有「依 seed 建世界」的入口，但
  `--tick N --data-dir D --save-dir S` 可以生世界；要帶 history 的槽就暫時附一支 gtest 到
  `tests/runtime/history_log_test.cpp` 尾端，跑完 `git checkout --` 還原（增量 build 很快）。

## 下一步

1. 叫 codex 起草 **M10.3b** 任務書（依[波 2 細目](runtime-injection-inject.md) M10.3b 節），
   管家按包絡與範本 `wf/inbox/done/m10-1-raws-in-save.md` 的骨架定稿 → 派工 → 親手驗收。
2. 同樣流程做 **M10.4**。3. **定版輪**：v23→24、同步三處 `static_assert`、彙總上面的格式清單、全套綠。
4. ⚠ 3b 動 `RulesetPatcher` 前先看 **A-1**（多角色槽重放對不上）——它會先炸。
