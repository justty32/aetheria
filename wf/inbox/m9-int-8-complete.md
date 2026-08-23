# M9-INT-8 完成回報 — 併入 M8.3 與 M9.2／M9.2b

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

## 結論與測試數分解

已在 `main` 依序合併 `m8-3-wt` 與 `m9-2-wt`，沒有改 `design/`，也沒有 push。
實際測試數與任務書推導完全相符，沒有增刪測試湊數：

- main 基準：**414**。
- M8.3 新增：**3**。
- 合併後 CTest：**N = 417 = 414 + 3**，417/417 全綠。
- M9.2／M9.2b pytest：**18/18** 全綠；它沒有掛入 CTest，故不計入 417。

指定 build 實際重新編譯 `lua_sandbox_test.cpp`、`playable_session_test.cpp` 與
`site_building_mapping_test.cpp`，不是 ninja 失敗後跑陳舊執行檔。build 原始 exit code 明確為
**0**，確認後才開始 ctest。

## 合併與衝突判定

兩條分支的 merge-base 都是 `3b63f17`。兩次 `git merge --no-ff` 都由 ort 自動完成，沒有
conflict marker；我仍逐項檢查預期交集：

- `wf/SESSION-LOG.md`：M8.3 只移除實作者區塊中已完成的 M8.2 陳舊條目；main 的規劃者
  區塊、M9 規劃與檔尾 `OPS-VERIFY.md` 連結全部保留。合併後為 7319 bytes，未超過上限，
  不需拆檔。
- `m9-2-wt` 只新增／修改 `tools/art/` 與它自己的收件匣文件，和 M8.3 core 內容沒有交集。
- M8.3 的 `target_sources(...)` 區塊完整加入 `targets_core.cmake` 與
  `targets_tests.cmake`；`cmake -S . -B build` exit 0，證明括號與 target 結構可解析。

`tools/art/verify_acceptance.py` 只補了 `fault_script` 找不到唯一字面目標時的
`RuntimeError` 診斷：提示被注入的那一行可能已修改，注入器也要更新。字面取代機制沒有重寫。
另以 `/dev/null` 作找不到目標探針，訊息包含新提示且 exit 0。

## 三組測試點名

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

M8.3（3 個 GoogleTest，實際為 #397／#398／#399）：

- `SiteBuildingMapping.EveryCityBuildingDefHasTheExpectedPersistentTypeAndWeight`
- `SiteBuildingMapping.HouseAndSettlementHallHaveDifferentObservableRegionEffects`
- `SiteBuildingMapping.ColdFileLoadDerivesTheSameTypeAndWeightFromDefinitionId`

`ctest -N` 與實際 ctest 執行清單都同時列出上述 15 條，沒有掉 target。

## 驗收表

| 項目 | 結果 |
|---|---|
| `cmake -S . -B build` | exit 0 |
| `cmake --build build --parallel 2` | exit 0；確認後才跑 ctest |
| `ctest --test-dir build --output-on-failure` | **417/417**，`414 + 3`，exit 0 |
| `python3 -m pytest -p no:cacheprovider tools/art/tests -q` | 暫存 venv 中 **18 passed in 4.29s**，exit 0 |
| `python3 tools/art/verify_acceptance.py` | exit 0 |
| `./build/aetheria_sim --tick 62208000` | exit 0；輸出四層 zone tree |
| `godot-mono --headless --path godot --quit-after 5` | exit 0 |
| `kSaveFormatVersion` | `core/serialize/zone_codec.h` 維持 **21** |
| `find . -name '*.md' -size +8k -print` | 空輸出 |
| `git diff --check` | 通過 |
| `design/` | 相對整合前 main 無變更 |

系統 `/usr/bin/python3` 首次跑 pytest 時因沒有 pytest 而 exit 1，訊息為
`No module named pytest`；這不是測試紅燈。我依 `.ask` 記錄的假設，在 repo 外 `/tmp` 建一次性
venv、安裝 `tools/art/requirements.txt`，啟用後原樣重跑指定命令取得上述 18/18；沒有改 repo
依賴或測試。驗收腳本的內建故障控制也實際執行：無序色盤令兩個 hash seed 分歧、跳過 terrain
旋轉令四個雜湊分歧、四種入庫規則各自停用後都由拒絕 exit 2 變成通過 exit 0。

## 現有驗收證不了的事

自動測試能證明兩邊 target 都被發現、建築映射／冷載入關係、Lua 與 runtime 護欄、美術工具的
合成 fixture 與故障控制通過；Godot headless 能證明主場景載入退出。但它不能證明實際滑鼠流程
可用、畫面可讀或真素材在人眼下風格一致。M9.2 目前仍只有佔位色盤與合成圖，真素材要經接觸表
與人工檢視。

M8.3 現有玩法仍沒有「建造領主廳」命令，因此住宅走完整施工 pipeline，領主廳效果則由既有
持久物件 fixture 驗證；不能把這說成兩者都有玩家施工流程。既有 Region formula 也仍沒有建築
組成輸入，故跨層 Development 校準缺口沒有由這次整合解決。
