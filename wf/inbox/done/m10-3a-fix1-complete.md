DONE
# M10.3a-fix1 已改為不可抵消的定序 FNV-1a 世界身分，UID 延遲配發缺陷已修正

## 結論

- 世界身分只由三分量組成：zone 正規化雜湊、槽內 raws 雜湊、日誌鏈頭；固定依 `zone → raws → head`，從 FNV offset 起依序把三個 little-endian u64 餵入同一條 FNV-1a。
- `world_state_hash` 與 `replay` 共用唯一的 `compose_world_identity`；replay 逐分量比對後才比合成值。
- History API 已移除可指定 genesis 的建構參數；空鏈只可能從 `kHistoryGenesisHash = 0` 起算。
- 新世界建構時一次 claim player army、enemy army、named commander 三個 preferred UID；具名指揮官使用已保留 UID，不再等到戰鬥時延遲 claim。

## 驗收 1：無分量 XOR、無校準常數、genesis 固定 0

```text
$ grep -n '\^' sim/world_hash.cpp
30:        hash ^= static_cast<std::uint8_t>(value & UINT8_MAX);
```

唯一 `^` 是 FNV-1a 對單一 byte 的標準步驟，沒有用來合成分量。

```text
$ rg 'kV23BaseRawsCompatibility' core sim tests cmake
（無輸出）
$ rg -n 'kHistoryGenesisHash' core/history
core/history/history_log.h:16:inline constexpr std::uint64_t kHistoryGenesisHash = 0;
core/history/history_log.cpp:114:    return entries_.empty() ? kHistoryGenesisHash : entries_.back().entry_hash;
core/history/history_log.cpp:152:    auto expected_prev = kHistoryGenesisHash;
```

任務書與前輪退回回報仍保留該名稱作歷史文字，依「不要重寫已過部分」未改；實作、測試與建置腳本已全空。

## 驗收 2：v23 zone、空鏈與現場獨立 fold

測試內另寫一份 FNV fold，沒有呼叫 production compose，也沒有寫死合成期望值：

```text
acceptance_hash zone_v23=17114528469974780418 zone_actual=17114528469974780418
acceptance_hash empty_head_seq=0 empty_head_hash=0
acceptance_hash folded_expected=2670718118062534143 composed_actual=2670718118062534143
```

實際 `aetheria_sim verify world-hash` 四行輸出：

```text
zone_hash=4698212455085739946 zone_count=4
raws_hash=17134265233666862005
history_head_hash=0 history_seq=0
world_hash=12771489023462499069 elapsed_ms=5.62116
```

單角色完整 replay 亦逐分量與合成值全等：

```text
stored_zone_hash=17087040534597837315 replay_zone_hash=17087040534597837315
stored_raws_hash=17134265233666862005 replay_raws_hash=17134265233666862005
stored_head_hash=4987398277836527920 replay_head_hash=4987398277836527920
stored_world_hash=2283017563559932410 replay_world_hash=2283017563559932410 history_seq=7
```

## 驗收 3：三分量與順序的真實負向控制

三個槽都從同一 baseline 複製；分別只改一格 zone 溫度、在槽內 raws 加一行 TOML 註解並同步 manifest raws hash、向真實 `history.log` append 一筆。每次觀察 RED 後均還原原始 bytes 再重算 GREEN；順序則交換 zone/raws 後恢復 canonical：

```text
acceptance_components zone_injected=RED before=2670718118062534143 after=16856903664141427063 zone_restored=GREEN actual=2670718118062534143
acceptance_components raws_injected=RED before=2670718118062534143 after=17689687494229799380 raws_restored=GREEN actual=2670718118062534143
acceptance_components head_injected=RED before=2670718118062534143 after=6802337650299527012 head_restored=GREEN actual=2670718118062534143
acceptance_components order_swapped=RED canonical=2670718118062534143 swapped=5402479279962074923 order_restored=GREEN actual=2670718118062534143
```

## 驗收 4：UID 延遲 preferred 路徑修前紅、修後綠

同一測試在新 session 後先合法配發 9002，再走移動、兩旬與戰鬥。修前確實在延遲 claim 9001 時硬拋：

```text
acceptance_uid delayed_claim=RED later_uid=9002 error=entity uid 已配發或為 0：9001
[  FAILED  ] HistoryAcceptance.NamedCommanderUidIsReservedBeforeLaterAllocations
```

改為建構時一次 claim 三個 preferred 後，同一路徑成功：

```text
acceptance_uid reserved=GREEN later_uid=9002
[       OK ] HistoryAcceptance.NamedCommanderUidIsReservedBeforeLaterAllocations
```

既有冷讀續配數字因 watermark 先跨過已保留的 9001，自然更新為：

```text
acceptance5 uid_sequence=1001,2001,9002,9003
```

## 全套與守門

```text
$ cmake --build build -j2
（通過）
$ ctest --test-dir build --output-on-failure --parallel 2
100% tests passed out of 431
Total Test time (real) = 51.86 sec
$ python3 tools/check_principle5.py
principle5 check passed: 54 populated enums (45 mechanisms, 9 debt)
$ git diff --check
（無輸出）
```

429 增為 431：新增「三分量／順序擾動與還原」及「named commander 預留 UID」兩項。首次全套只因 `check_sim_world_hash.cmake` 尚解析舊單行輸出而紅；同步四行工具契約後該項單獨綠，再跑全套 431 綠。`kSaveFormatVersion` 仍是 23，未開 enum，未動 `design/`、RulesetPatcher、`add_faction`、`inject/`，未 push。

## 存檔格式異動清單增修（供整合輪定 v24）

- `history.log`：仍是前輪新增的獨立二進位檔與相同欄位版型；**修正鏈語意**為第一筆 `prev_hash = 0`，不再由 raws 播種。前輪以 raws genesis 寫出的鏈不相容，整合輪須視為格式語意變更。
- `history.commit`：前輪新增的獨立 little-endian u64，這輪未改。
- `data/civilization.toml`：前輪新增三個 preferred UID，這輪未改檔；**修正配發語意**為新世界建構時三個全 claim，因此既有 manifest 欄位 `next_entity_uid` 初始推進至 9002。未新增／重排 manifest 欄位。
- `sim/world_hash.{h,cpp}`：世界身分外部語意改為三分量定序 FNV-1a，驗證工具輸出改四行；不屬磁碟 codec。
- `cmake/check_sim_world_hash.cmake`：只同步驗證工具輸出契約，不屬存檔格式。

## 無法只靠 seed＋基底 raws＋日誌重建的清單

1. **多角色槽的角色選取／匯入生命週期**尚不是玩家日誌條目。實測既有雙角色槽完整重放得到 `stored=10773397634216242476`、`replay=10707280563954706049`（history seq 10）；即使每筆 payload 帶 residence/座標/玩家部隊/任務 context，角色 save 的選取與 zone materialization 軌跡仍無法由目前三項輸入位元級重建。`sim replay` 本輪只證明任務要求的單角色命令線。
2. **被 session 拒絕的玩家命令**依 journal-first 原則仍會先留下輸入，但日誌沒有「拒絕結果／未套用」條目；若狀態脈絡不足，重放會在該筆再次丟例外，無法自行判定應略過或視為有效輸入。

完成前已再次讀取 `.codex-inbox/`：沒有 `m10-3a-fix1.reply`，現有 `m10-3a.reply` 裁定未變；本輪沒有被迫假設。
