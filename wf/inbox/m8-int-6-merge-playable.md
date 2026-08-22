# 任務書 M8-INT-6 — 併入 M8.1 可玩迴圈

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**必讀協定**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)
**基準**：main（已含 M7 全部）

---

## 只做整合

`git merge --no-ff m8-1-wt` 在 `cmake/targets_core.cmake` 有衝突。

⚠ **括號陷阱（本 repo 踩過六次）**：`target_sources(...)` 的開頭在衝突塊內、`)` 在塊外。
批次「兩邊都取」會漏掉結尾括號，**而且錯誤訊息離病灶很遠**——
我這次實測到的是 CMake 的
`Parse error. Expected a command name, got unquoted argument with text`。

**先補完前一塊的 `)`，再開新的獨立區塊。逐段解，不要批次。**
INT-3／INT-4／INT-5 都示範過。

## ⚠ 這輪是 M8 的判準所在，合併後要親自確認迴圈還活著

M8 的判準是**唯一無法靠測試自證**的那條：

> 不看程式碼、不跑測試，只用滑鼠鍵盤，能打完一場仗並看到世界因此改變。

所以合併後**要把 M8.1 的操作腳本從頭走一遍**，確認七個步驟都還在，
並**重新產生兩張截圖**（親自指揮／系統計算）放回 `godot/artifacts/`。

⚠ 合併很容易把 Godot 端弄壞而測試全綠——**截圖是唯一的證據**。

## 驗收

| 判準 | 怎麼量 |
|---|---|
| 衝突解掉 | `git diff --check` 過 |
| 全套綠 | `ctest` 數字 |
| **迴圈還活著** | 兩張新截圖 + 操作腳本七步逐步確認 |
| 批次拉圖仍是一次呼叫 | 附 bytes 與呼叫次數 |
| 場景 free 後重建一致 | 附比對結果 |
| bridge 仍擋得住域外輸入 | 域外座標與域外 tick 各一，Godot 不掛 |

## 不要做的事

改演算法或 UI 行為｜動 `kSaveFormatVersion`（維持 20）｜改 `design/`

## 回報

`wf/inbox/m8-int-6-merge-playable-complete.md`：衝突怎麼解、
七步確認結果、兩張截圖路徑、批次拉圖數字、重建比對、bridge 擋輸入的兩個實測、`ctest` 數字。

## 規約

- `cmake --build build --parallel 2`｜不准 fan-out 自我審查子 agent｜不要改 `design/`｜
  不要 push｜繁體中文、每份文件 ≤ 8 KB
