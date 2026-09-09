DONE — M10 波 0 三線已整合，v22 定版與 420 項測試全綠

## 合併結果

- 依序併入 `m10-0a`（`e89e1aa`）、`m10-0b`（`8a4867c`）、`m10-0c`（`3d39855`）。
- 對應 merge commit：`e125c05`、`1d34da9`、`8a412e7`；三個來源 commit 均為 HEAD ancestor。
- 沒有產生 Git conflict，也沒有手動改寫三線交付內容。

## 衝突逐條

- 無需手動解衝突。`cmake/targets_tests.cmake` 的共同追加由 `ort` 自動整合；人工檢查後，
  `session_persistence_test.cpp` 與 `principle5.ContentKindsAreData` 都有註冊，`add_test(...)`
  定義區閉括號完整，沒有「被切開的括號」。
- 解衝突所需最小改動：無。

## 交互影響

- principle5 會掃 0c 新標頭；`core/world/army_state.h` 與
  `core/runtime/session_persistence.h` 均沒有 enum，因此沒有新增帶枚舉子 enum，也不需修改白名單。
- 0b 改寫的 `tests/zone/file_zone_store_test.cpp` 保持原樣併入；與 0c 的
  `FileZoneStore`／manifest／抽象 store 改動可共同編譯。全套通過，另跑
  `ctest -R '^FileZoneStore\.'` 為 13/13 通過。

## 建置與測試

首次 configure 因本機 Godot 4.7.2 與專案固定 4.7.1 不符；依 `design/architecture/build.md` 的 fallback，
並沿用已驗收 0c 的設定，以 `-DAETHERIA_GODOT_BIN=/usr/bin/false` 使用 godot-cpp 內建 4.7 API。
此 worktree 亦先初始化既定 submodule gitlink `d7b6162…`；兩者都沒有修改 repo 內容。

```text
$ cmake --build build -- -j2
[1342/1345] Linking CXX executable aetheria_sim
[1343/1345] Linking CXX executable aetheria_tests
[1344/1345] Linking CXX shared library .../godot/bin/libaetheria_bridge.so
（exit 0）

$ ctest --test-dir build --output-on-failure --parallel 2
419/420 Test #420: VerificationBoundary.WorldHash ... Passed
420/420 Test #417: SimWorldgen.DumpAndVerify ...... Passed
100% tests passed out of 420
Total Test time (real) = 49.16 sec
```

總數推導為基線 417 + 0a 的 principle5 1 項 + 0c 的 session persistence 2 項 = 420；
高於 0a 的 418 與 0c 的 419，沒有漏測。

獨立重掃：

```text
$ ctest --test-dir build -R principle5 --output-on-failure --parallel 2
1/1 Test #415: principle5.ContentKindsAreData ... Passed 0.06 sec
100% tests passed out of 1
```

CLI 新建槽後直接驗證：

```text
$ ./build/aetheria_sim --tick 62208000 --save-dir <新建暫存槽>
$ ./build/aetheria_sim verify world-hash <新建暫存槽>
world_hash=8485915008936685094 zone_count=4 elapsed_ms=0.582641
```

## v22 佐證

`kSaveFormatVersion = 22` 的定義恰好一處；三個版本 static assertion 均為 22，
`core/`、`tests/` 的 `== 21` 殘留為零。任務指定 grep 輸出關鍵列如下：

```text
core/serialize/zone_codec.h:13:inline constexpr std::uint32_t kSaveFormatVersion = 22;
tests/zone/zone_codec_test.cpp:103:    static_assert(kSaveFormatVersion == 22);
tests/site/site_building_mapping_test.cpp:149:    static_assert(aetheria::serialize::kSaveFormatVersion == 22);
tests/site/site_observation_persistence_test.cpp:110:    static_assert(aetheria::serialize::kSaveFormatVersion == 22);
```

`grep -rn "kSaveFormatVersion\|== 21" core/ tests/` 另只列出上述符號的正常編解碼、manifest、
store 與測試引用，沒有任何 `== 21`。
