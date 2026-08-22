# 任務書 M7-INT-3 — 把 M7.1 地城併進 main

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**必讀協定**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)
**基準**：main（已含 M6.7 + M7.0）

---

## 這輪只做整合，不改行為

`m7-1-wt`（地城）branch 自 M7.0 併入之前，所以 `git merge --no-ff m7-1-wt` 會產生
**七個衝突**：

```
cmake/targets_core.cmake          cmake/targets_tests.cmake
core/rules/ruleset.cpp            core/rules/ruleset.h
tests/rules/ruleset_test_support.h
tests/worldgen/worldgen_test_support.h
wf/workflows/common/code-map.md
```

## ⚠ 括號陷阱：這個 repo 已經踩過四次

衝突區常常**把函式主體或 `target_sources(...)` 的括號切開**——
`{` 或 `(` 在衝突塊內、結尾在塊外。用「兩邊都取」的通則批次解會**漏掉結尾括號**。

⚠ **而且編譯器的報錯離病灶很遠**：實測看到的是
`aetheria::rules::aetheria::world::RegionTiles has no member named 'edges'`（命名空間套疊），
完全指不到出事的檔。

分類處理：

| 區 | 怎麼解 |
|---|---|
| **宣告區**（`.h` 的成員與方法宣告） | 兩邊直接串接，安全 |
| **定義區**（`.cpp` 的函式本體） | 兩邊之間要補 `}`，不能只串接 |
| **CMake `target_sources(...)`** | 兩邊之間要補 `)` |

**逐個看清楚再解，不要整批 sub。**

## 版本號

M7.0 沒有用掉 v19（它沒加持久欄位），M7.1 已用 **v20**。
合併後就是 **v20**，不要再動。

## 驗收

| 判準 | 怎麼量 |
|---|---|
| 七個衝突都解掉 | `git diff --check` 過 |
| 兩邊的功能都在 | M7.0 的三條位階來源測試與 M7.1 的地城測試**都要綠** |
| 全套綠 | `ctest` 數字 |
| 沒有改行為 | 只解衝突，不改演算法或參數；若你不得不改，如實回報改了什麼與為什麼 |

**負向控制**：合併後跑一次 M7.0 的「打斷共用接法 → 三條來源同時紅」，
確認整合沒有把那個結構性證據弄壞。附紅的三個測試名。

## 不要做的事

| 不要 | 理由 |
|---|---|
| 改演算法或平衡值 | 這輪只做整合 |
| 動版本號 | 已是 v20 |
| 改 `design/` | 我裁定 |

## 回報

`wf/inbox/m7-int-3-merge-dungeon-complete.md`：七個衝突各自怎麼解、
負向控制的三個測試名、`ctest` 數字、你不得不改的東西（若有）。

## 規約

- `cmake --build build --parallel 2`｜不准 fan-out 自我審查子 agent｜不要改 `design/`｜
  不要 push｜繁體中文、每份文件 ≤ 8 KB
