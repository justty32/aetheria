# M8.0 完成回報 — Lua 沙箱與六條決定論鐵律

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者

## 落地內容

- vcpkg 加入 sol2 3.5.0，Lua 以 override 固定 5.4.8；registry 預設已升 5.5 的處理已寫
  `.codex-inbox/m8-0.ask`。
- `core/script/` 新增逐次建立 `_ENV` 的引擎、受限 `Context`、`ctx.rng:int(a,b)`、
  Region owner 綁定與事件化 transaction。
- 掛勾已實作事件 `condition/effect`、劇情 `on_trigger`、勝敗 `check -> Outcome`；
  `RegionTurnPipeline` 只在 stage 6 `Events` 同步呼叫腳本 pass。
- 熱重載先在候選 Lua state 載入、lint、驗證現存物件所需 id，全部成功才交換；拒絕時
  原 Lua catalog 與世界狀態都不動。
- 沒有修改 `design/`、`bridge/`、`godot/` 或 `kSaveFormatVersion`（仍為 20）。

## 六條鐵律：獨立正向測試與獨立負向控制

| # | 獨立測試 | 正向 | 單獨故障注入後確實紅燈 |
|---|---|---|---|
| 1 | `LuaDeterminismLaw1.SandboxEnvironmentHasOnlyTheCompleteAllowlist` | 禁止來源全為 nil，完整 allowlist 相等 | 開回 `os` library 並放入 `_ENV`：keys 多出 `os`，condition `false`，1 test failed |
| 2 | `LuaDeterminismLaw2.InjectedRngIsTheOnlySourceAndMatchesCoreMt19937_64` | 注入值 expected/actual=`52857/52857` | 額外暴露 `random_source=math.random`：condition actual `false`、expected `true` |
| 3 | `LuaDeterminismLaw3.OrderedPairsSortsKeysAndLintRejectsRawPairs` | `a,m,z` 寫出 owner `123`，raw `pairs` 被 lint 拒絕 | `ordered_pairs` 改成 `pairs`：owner actual `231`、expected `123` |
| 4 | `LuaDeterminismLaw4.FreshEnvironmentDropsCrossTurnStateAndSaveOmitsLuaState` | 兩次 effect 後 counter 仍 `1`、事件僅 1 筆 | 共用持久 `_ENV`：owner actual `3`、expected `1`；events actual `2`、expected `1` |
| 5 | `LuaDeterminismLaw5.PipelineRunsSynchronousHooksOnlyAtTheEventsStage` | callback stages=`{6}`，且 coroutine/async 不可見 | 在 stage 4 加掛一次：actual stages=`{4,6}`、expected=`{6}` |
| 6 | `LuaDeterminismLaw6.ErrorAbortsSettlementReportsItAndDoesNotCommit` | owner 仍 0、事件 0 筆，錯誤含來源與 Lua traceback | 將 error path 改成 `return`：紅字「腳本錯誤被靜默吞掉」，1 test failed |

六次控制均一次只改一條、跑該條 filter、取得上述紅燈後還原；還原後全量測試綠燈。

## `_ENV` 完整 key 清單

實測排序後共 16 個：

```text
_ENV,_VERSION,assert,error,ipairs,math,next,ordered_pairs,outcome,select,string,table,tonumber,tostring,type,utf8
```

`os`、`io`、`collectgarbage`、`pairs`、`coroutine`、`package`、`require` 都不在；複製出的
`math` 表也沒有 `random`、`randomseed`。每次掛勾另建環境並複製安全 table，腳本不能藉修改
`math` 等表污染下一次呼叫。

## 其餘驗收證據

- **Context 型別隔離雙向證據**：`ScriptContextIsolation.WorldTruthCompileFailure` 先只給
  `core/script/include`，因找不到 `core/script/context_internal.h` 而失敗；再加入 repo root
  後同一檔編譯成功。檢查會清掉環境 `CPLUS_INCLUDE_PATH/CPATH`，不依 cwd 假通過。
- **決定性**：`LuaSandbox.SameSeedAndScriptProduceTheSameWorldHash` 的兩次 hash 都是
  `4497833651549280861`。
- **存檔沒有 Lua state**：腳本全域放入 `LUA_STATE_SENTINEL_M8` 後，Region 存檔為
  2,103 bytes，binary search offset=`18446744073709551615`（`string::npos`）；Lua state、
  source、catalog 都不在 codec/`AllComponents`，版本號未動。
- **熱重載原子拒絕**：缺 `reload.required` 時錯誤為
  「拒絕熱重載：現存物件引用缺少的 Lua id：reload.required」；拒絕前後世界 hash 都是
  `6107762979157215260`，owner、事件與舊 catalog 也逐項相等。
- **每回合呼叫數**：condition=`1`、effect=`1`、narrative=`1`、victory=`1`，total=`4`，
  遠低於 10,000 防線。
- **效能**：固定暖機一次，N=5 取最小，完整四掛勾回合 `0.21436 ms`。
- **完整驗證**：`cmake --build build --parallel 2` 成功；
  `ctest --test-dir build --output-on-failure` 為 `412/412`，87.03 秒；
  `./build/aetheria_sim --tick 62208000` exit 0。

## 現有測試證不了的事

- 這輪提供明確的同步 `reload_directory()`，沒有實作檔案監看器；何時偵測 `scripts/` 變更由
  上層開發工具決定。
- 沙箱隔絕非決定 API，但尚無 Lua instruction/memory quota；惡意無窮迴圈或記憶體耗盡不是
  本輪六條鐵律能證明的安全性。
- 技能、建築、AI、生成後修飾掛勾按任務範圍未實作；現有 enum/definition catalog 可延伸，
  但其領域 API 要由後續任務定義。
- M8.1 的 Godot/bridge 入口與實機熱重載操作不在本輪範圍；本輪只驗證純 C++ core。
