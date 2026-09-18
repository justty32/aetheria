# M7-INT-3 地城整合完成回報

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**原任務**：[`m7-int-3-merge-dungeon.md`](m7-int-3-merge-dungeon.md)

## 結果

已以 `git merge --no-ff m7-1-wt` 把 M7.1 地城併入 `m7-int3-wt`。七個預期衝突全數
解決，M7.0 三條位階來源與 M7.1 地城功能同時保留。存檔版本維持 **v20**，
沒有改演算法、參數或 `design/`，沒有 push。

## 七個衝突的解法

| 檔案 | 解法 |
|---|---|
| `cmake/targets_core.cmake` | 保留 M6.7 戰鬥校準、M7.0 `power_sources` 與 M7.1 地城來源；在 M7.0 `target_sources(...)` 後補獨立 `)`，再開地城區塊。 |
| `cmake/targets_tests.cmake` | 保留 M6.7 負向 target、M7.0 測試與信仰隔離護欄，先補完 `add_test(...)` 的 `)`，再以獨立 `target_sources(...)` 加入地城測試。 |
| `core/rules/ruleset.cpp` | loader 編排依序保留 `load_power_source_rules` 與 `load_dungeon_rules`；保留學派／教義／神祇／種族與機關 accessor，並在 `find_race()` 後補回被衝突切開的 `}`。 |
| `core/rules/ruleset.h` | 宣告區直接串接兩邊：同時保留兩個 include、public accessor、private 容器／索引／規則與兩個 loader 宣告。 |
| `tests/rules/ruleset_test_support.h` | fixture 同時複製 `power_sources.toml` 與 `dungeon.toml`。 |
| `tests/worldgen/worldgen_test_support.h` | worldgen fixture 同時複製兩份 TOML，並為地城複製補上獨立 `copy_file(...)` 呼叫開頭，避免兩邊只串接後形成斷裂呼叫。 |
| `wf/workflows/common/code-map.md` | 採 M7.1 的 `ruleset_load_history*.cpp` 精簡總稱，下方既有 history detail 約束仍在；保留 M7.1 地城 rules 導航列，合併後檔案維持 8 KiB 內。 |

## 負向控制（真的紅，已還原）

暫時讓唯一共用接點 `resolve_power_profile()` 直接回傳基礎 profile，重新以
`cmake --build build --parallel 2` 建置後，聚焦 CTest 為 exit **8**、**0/3** 通過。
三個同時紅的測試名為：

1. `PowerSourceConnector.MagicUsesTheSharedTierAbilityAndAttributePath`
2. `PowerSourceConnector.FaithUsesTheSharedTierAbilityAndAttributePath`
3. `PowerSourceConnector.BloodlineUsesTheSharedPathAndEnforcesItsCap`

實際斷點分別為：魔法 tier `Ambient/Site`、mind `40/47`、能力空／
`ability.magic.local`；信仰 tier `Local/Region`、spirit `30/39`、能力空／
`ability.faith.domain`；血統 tier `Site/Local`、body `20/26`、能力空／
`ability.race.innate`。還原並重建後同三測 **3/3** 通過，暫改無殘留。

## 驗證

- `cmake --build build --parallel 2`：clean worktree 全量建置成功；後續故障注入與還原也均固定兩路。
- M7.0 三條來源 + M7.1 地城聚焦測試：**16/16** 通過。
- `ctest --test-dir build --output-on-failure`：**385/385** 通過，82.79 秒。
- `./build/aetheria_sim --tick 62208000`：exit 0，抵達第 3 年第 1 季第 1 月上旬。
- Godot 4.7.1 headless editor：全新首掃完成後出現已知 exit 139；原樣重跑 exit 0。主場景 exit 0。
- `git diff --check`：通過；無未解衝突，`design/` 無異動。

## 不得不改的東西

無。正式差異只來自 M7.1 branch 與上述衝突解法；沒有額外行為、演算法、
平衡參數或版本號變更。
