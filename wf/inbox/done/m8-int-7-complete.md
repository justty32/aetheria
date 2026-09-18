# M8-INT-7 完成回報 — 合併 M8.2

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

## 結論與測試數差異

已在 `main` 合併 `m8-2-wt`，保留 M8.0 Lua 與 M8.2 runtime 兩邊功能。完整重建後
CTest 為 **414/414 全綠**，但這低於任務書指定的 **N ≥ 420**；沒有新增空殼或重複測試
硬湊數字，已透過 `.codex-inbox/m8-int-7.ask` 即時回報並採用「完整聯集 414 為正確結果」
的假設繼續。

三個數字與實際組成如下：

- 合併前 `main = 412`：共同基線 402 + M8.0 Lua 10。
- `m8-2-wt = 404`：共同基線 402 + M8.2 runtime 2。
- 合併後 `N = 414`：共同基線 402 + M8.0 Lua 10 + M8.2 runtime 2。

M8.2 的 `tests/runtime/playable_session_test.cpp` 實際只有兩個 `TEST(...)`；因此兩分支的
聯集是 414，不是 420。這不是 target 遺失：兩份新增來源都在同一次完整 build 中實際編譯，
CTest 清單也同時列出下列 12 個新增測試。

## 點名測試

M8.0 Lua（9 個 GoogleTest + 1 個 compile-failure CTest）：

- `LuaDeterminismLaw1.SandboxEnvironmentHasOnlyTheCompleteAllowlist`
- `LuaDeterminismLaw2.InjectedRngIsTheOnlySourceAndMatchesCoreMt19937_64`
- `LuaDeterminismLaw3.OrderedPairsSortsKeysAndLintRejectsRawPairs`
- `LuaDeterminismLaw4.FreshEnvironmentDropsCrossTurnStateAndSaveOmitsLuaState`
- `LuaDeterminismLaw5.PipelineRunsSynchronousHooksOnlyAtTheEventsStage`
- `LuaDeterminismLaw6.ErrorAbortsSettlementReportsItAndDoesNotCommit`
- `LuaSandbox.SameSeedAndScriptProduceTheSameWorldHash`
- `LuaSandbox.MissingIdRejectsReloadAtomicallyWithoutTouchingWorld`
- `LuaSandbox.DirectoryReloadRebuildsStateAndPerformanceUsesMinOfFive`
- `ScriptContextIsolation.WorldTruthCompileFailure`

M8.2 runtime（2 個 GoogleTest）：

- `PlayableCoverage.ThreeLayersReturnAndWriteAuthoritativeParentValues`
- `PlayableCoverage.ManualAndManagedCityExpectationMatchesAtOneHundred`

## 兩處衝突判定

### `cmake/targets_tests.cmake`

保留 M8.0 的 `tests/script/lua_sandbox_test.cpp` 與
`ScriptContextIsolation.WorldTruthCompileFailure`，並在完整關閉 `add_test(...)` 後追加 M8.2
的 `tests/runtime/playable_session_test.cpp`。未直接拼接 marker 內容；衝突解完後粗檢全檔
括號為 open 42 / close 42，且 `cmake -S . -B build` 成功解析。

### `wf/workflows/common/code-map.md`

保留 M8.2 新增的 `runtime/`、`narrative/` 測試導航，也恢復 Git 在 conflict marker 外自動
套用刪除的這段有效描述：`gen_stage_ids.h`、`gen_grid.h`、`gen_noise.h`、`gen_hash.h` 與
`biome_classification.h`。合併後主檔為 8262 bytes，超過 8 KiB，因此依文件鐵律把完整
`core/worldgen` 主題搬至 `code-map-worldgen.md`，主檔保留連結；內容沒有刪減。

## 驗收

| 項目 | 結果 |
|---|---|
| `cmake -S . -B build` | exit 0 |
| `cmake --build build --parallel 2` | exit 0；先確認後才跑 ctest |
| `ctest --test-dir build --output-on-failure` | 414/414 全綠；**未達任務書 N ≥ 420，原因如上** |
| `./build/aetheria_sim --tick 62208000` | exit 0，輸出三層 zone tree |
| `godot-mono --headless --path godot --quit-after 5` | exit 0 |
| `kSaveFormatVersion` | 維持 20 |
| `design/` | 無變更 |
| `git diff --check` | 通過 |
| `find . -name '*.md' -size +8k -print` | 空輸出 |

## 現有驗收證不了的事

自動測試能證明兩邊 target 都被發現、core 三層進退／回寫／期望值與 Lua 決定論護欄通過，
Godot headless 能證明主場景可載入退出；但它不能證明實際滑鼠可點性、各層畫面可讀性、截圖
仍與目前環境逐像素一致，或互動期間永遠不會出現 signal emission 中同步 free。這些仍依賴
M8.2 原回報的實機滑鼠流程與截圖證據。測試也不能支持「至少 420」這個數字：repo 中兩個
M8.2 runtime case 加上十個 M8.0 case 的實際聯集就是 414。

未修改 `design/`，未變更存檔格式，未 push。
