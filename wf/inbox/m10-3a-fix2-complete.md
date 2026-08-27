DONE
# M10.3a-fix2 已以基底＋完整日誌重建消除重複套用窗口，world-hash 控制已真正斷言

## 結論

- `head > marker` 或 marker 缺失時，不再把尾巴套到來源不明的現役 zone；改從槽內 raws＋manifest seed/region 建立乾淨世界，重放完整日誌，再覆寫 zone／manifest／目前角色檔，最後才原子前推 marker。
- `check_sim_world_hash.cmake` 現在解析兩槽四分量，斷言本擾動下 `raws_hash`、`world_hash` 必須不同，`zone_hash` 必須相同；失敗訊息列出兩邊 zone/raws/head/world。
- fix1 的 `compose_world_identity`、genesis=0 與 `zone → raws → head` 定序 fold 完全未改；`kSaveFormatVersion` 仍為 23。

## 驗收 1：存完 zone/manifest、marker 未前推

新增真鉤子停在 zone／manifest 與角色檔都成功落盤之後、marker 前推之前；測試先建住宅再停住，磁碟確實是已套用世界＋marker 0。修前沿用舊 `replay_tail`，重複套用同一筆 `build_city` 而紅：

```text
acceptance_fix2 post_save_interrupted=RED error=M10.3a 測試鉤：zone/manifest 已落，marker 未前推
unknown file: Failure
C++ exception with description "示範住宅已經蓋過" thrown in the test body.
[  FAILED  ] HistoryAcceptance.WorldSaveBeforeMarkerInterruptionRebuildsWithoutDuplicateApply
```

修後同一路徑由乾淨基底重放完整 3 筆，能開槽，且與不中斷路徑逐位元相等：

```text
acceptance_fix2 initial_marker_missing=GREEN history_seq=0
acceptance_fix2 post_save_interrupted=RED error=M10.3a 測試鉤：zone/manifest 與角色檔已落，marker 未前推
acceptance_fix2 post_save_recovered=GREEN recovered_hash=273835825294284374 direct_hash=273835825294284374 history_seq=3
[       OK ] HistoryAcceptance.WorldSaveBeforeMarkerInterruptionRebuildsWithoutDuplicateApply
```

原驗收 3（append 完成、套用前中止）保留且仍綠：

```text
acceptance3 interrupted=RED error=M10.3a 測試鉤：日誌與 marker 已落，套用未跑
acceptance3 recovered=GREEN recovered_hash=7329817757515979101 direct_hash=7329817757515979101
[       OK ] HistoryAcceptance.JournalBeforeApplyInterruptionRecoversBitExactly
```

### 為何涵蓋 commit 內全部崩潰點

1. append 完成後到任何玩法套用點：marker 仍舊；下次忽略現役 zone，從基底完整重放。
2. zone 逐檔原子替換途中、manifest 前後、全部 zone/manifest 已存：不論磁碟是舊、混合或新狀態，marker 仍舊；同樣完整重建覆寫，故不會重複套用。
3. 另發現舊順序在 marker 前推後才寫角色檔，會形成「新世界＋誠實 marker＋舊角色」。現在角色檔原子替換也在 marker 前；其前後崩潰皆因舊 marker 而重建。
4. marker 原子替換時崩潰只有舊值或新值：舊值走完整重建；新值代表 zone／manifest／角色檔皆已完成，直接冷讀。
5. 另發現首次空日誌存檔若 marker 尚未建立，舊判斷會因 `head == marker == 0` 誤認已提交。現在「marker 缺失」本身即需恢復；測試也刪除 marker 與角色檔，從 genesis 重建成功。

## 驗收 2：world-hash 控制真的會紅

把腳本的替換結果由 `move_cost = 9` 真改回 `3`，讓兩槽相同；新增分量比較命中而紅，並列出兩邊四分量：

```text
CMake Error at cmake/check_sim_world_hash.cmake:79 (message):
  terrain move_cost 擾動未改變
  raws_hash：old={zone=4698212455085739946, raws=17134265233666862005,
  head=0, world=12771489023462499069}; new={zone=4698212455085739946,
  raws=17134265233666862005, head=0, world=12771489023462499069}
0% tests passed, 1 tests failed out of 1
```

改回 `9` 後綠；數字同時證明 zone 對此擾動無感，raws 與合成值有感：

```text
舊槽：zone_hash=4698212455085739946 raws_hash=17134265233666862005 history_head_hash=0 world_hash=12771489023462499069
新槽：zone_hash=4698212455085739946 raws_hash=10776467794636923675 history_head_hash=0 world_hash=11501016914046886720
100% tests passed out of 1
```

## 全套與守門

```text
$ cmake --build build -j2
（通過）
$ ctest --test-dir build --output-on-failure --parallel 2
100% tests passed out of 432
Total Test time (real) = 53.59 sec
$ python3 tools/check_principle5.py
principle5 check passed: 54 populated enums (45 mechanisms, 9 debt)
$ git diff --check
（無輸出）
```

431 增為 432：只新增「zone/manifest 已存、marker 未推時從完整日誌重建」一項；原套用前窗口仍是既有測試。未開 enum，未動 `design/`、RulesetPatcher、`add_faction`、`inject/`、SQLite 或截斷重放，未 push。

## 存檔格式異動清單增修（供整合輪定 v24）

- `history.log`：位元格式、genesis=0、雜湊鏈與 append-only 語意未改。
- `history.commit`：仍是獨立 little-endian u64，沒有塞進 manifest；增修交易語意為 marker 最後寫，缺檔代表未提交，舊值代表需由完整日誌重建。
- `chars/<角色>.bin`：codec／版本未改；只把原子寫入順序移到 marker 前。
- zone／manifest codec 與欄位皆未改；`kSaveFormatVersion == 23`。
- `cmake/check_sim_world_hash.cmake` 只加驗證斷言，不屬磁碟格式。

## 本輪另發現、現有限制下仍無法自動恢復的崩潰點

1. 首次建槽若崩在完整 manifest 出現以前（例如 raws 複製中途，或已有 `.bin` 但尚無 manifest），磁碟尚無可稽核的 seed／region 基線，`FileZoneStore` 會 fail-fast；本輪能恢復的是 manifest 已存在後到 marker 的所有窗口。若要連這段也自動恢復，需另立建槽交易／基線 metadata，超出本輪兩問題且不能偷塞 manifest 欄位。
2. `history.log` 單筆 append 本身若被 kill 成截斷尾筆，整鏈驗證會 fail-fast；在「append-only、禁止截斷重放」約束下不能把破尾當成已完成輸入。這是 append 成功以前的邊界，不是本輪已完成 append 後的重複套用窗口。

## 無法只靠 seed＋基底 raws＋日誌重建的清單（照抄保留）

1. **多角色槽的角色選取／匯入生命週期**尚不是玩家日誌條目。實測既有雙角色槽完整重放得到 `stored=10773397634216242476`、`replay=10707280563954706049`（history seq 10）；即使每筆 payload 帶 residence/座標/玩家部隊/任務 context，角色 save 的選取與 zone materialization 軌跡仍無法由目前三項輸入位元級重建。`sim replay` 本輪只證明任務要求的單角色命令線。
2. **被 session 拒絕的玩家命令**依 journal-first 原則仍會先留下輸入，但日誌沒有「拒絕結果／未套用」條目；若狀態脈絡不足，重放會在該筆再次丟例外，無法自行判定應略過或視為有效輸入。

完成前已再次讀取 `.codex-inbox/`：沒有 `m10-3a-fix2.reply`，現有 `m10-3a.reply` 裁定未變；本輪沒有被迫假設。
