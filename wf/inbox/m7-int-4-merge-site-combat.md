# 任務書 M7-INT-4 — 把 M7.2 Site 方陣戰鬥併進 main

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**必讀協定**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)
**基準**：main（已含 M6.7 + M7.0 + M7.1）

---

## 只做整合

`git merge --no-ff m7-2-wt` 會在兩個檔產生衝突：
`cmake/targets_core.cmake`、`cmake/targets_tests.cmake`。

⚠ **括號陷阱（本 repo 踩過五次）**：衝突區常把 `target_sources(...)` 或
`add_test(...)` 的括號切開——開頭在塊內、`)` 在塊外。
「兩邊都取」的通則批次解會漏掉結尾括號，**而編譯器報錯離病灶很遠**
（實測是命名空間套疊，完全指不到出事的檔）。**逐個看清楚再解。**

M7-INT-3 已示範過正確做法：先補完前一塊的 `)`，再開新的獨立區塊。

## 版本號

M7.2 沒有新增持久欄位，**版本維持 v20**，不要動。

## ⚠ 合併後一定要確認這條還活著

M7.2 補上了一條我要的斷言，它堵的是 M6.7 的一個真實漏洞：

```
Site−Region 的 A、B、總損失三組誤差不得全部同向
```

背景：M6.7 唯一的守門是 `|error| < 5%`，我注入 **Site 系統性 +3% 時它全綠**，
+10% 才紅。3% 的單向偏差正是玩家學得會、而測試看不見的那種。

**負向控制**：合併後在 `site_combat` 的 `loss_a`／`loss_b` 各加 3%（同向），
確認 `CombatScaling.ExpectationParityCoversBalancedRatiosWithoutSuppressingVariance`
**變紅**，並附 `signed_errors_site_region_A_B_total` 的三個值。做完還原。

## 驗收

| 判準 | 怎麼量 |
|---|---|
| 兩個衝突解掉 | `git diff --check` 過 |
| 三邊功能都在 | M7.0 位階來源、M7.1 地城、M7.2 Site 戰鬥的測試都綠 |
| 同向偏差斷言活著 | 上述負向控制的三個數字 |
| 全套綠 | `ctest` 數字 |
| 沒改行為 | 只解衝突；不得已改了什麼要如實回報 |

## 不要做的事

改演算法或平衡值｜動版本號｜改 `design/`

## 回報

`wf/inbox/m7-int-4-merge-site-combat-complete.md`：兩個衝突怎麼解、
同向偏差負向控制的三個值、`ctest` 數字。

## 規約

- `cmake --build build --parallel 2`｜不准 fan-out 自我審查子 agent｜不要改 `design/`｜
  不要 push｜繁體中文、每份文件 ≤ 8 KB
