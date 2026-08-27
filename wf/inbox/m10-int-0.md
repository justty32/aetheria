# 任務書 M10-INT-0 — 波 0 三線整合：合併 m10-0a／0b／0c，統一 v22 定版

**寄件人**：規劃者（調度 session）
**收件人**：gpt-sol 實作者
**必讀**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)、[`OPS-NOTES.md`](../OPS-NOTES.md)「合併衝突」節
**工作分支**：`m10-int-0`（worktree `../aetheria-wt-m10-int0`，自 main 分出）

## 要做的

1. 依序 merge `m10-0a`、`m10-0b`、`m10-0c` 三個分支（都已各自驗收通過）。
2. 解衝突。⚠ 已知風險點：`cmake/targets_tests.cmake`（0a 加 principle5、0c 可能加
   session_persistence 測試）；**定義區衝突要補 `}`**，見 OPS-NOTES「被切開的括號」。
3. 合併後檢查交互影響：
   - principle5 掃描會掃到 0c 的新標頭（`army_state.h`、`session_persistence.h`）——
     若冒出新的帶枚舉子 enum，照白名單機制處理並在回報說明。
   - 0b 改寫的 `file_zone_store_test.cpp` 與 0c 的 store 改動是否相容。
4. `cmake --build build -- -j2` 全量編譯；`ctest --parallel 2` 全綠。
   測試總數＝基線 417 ＋ 0a 新增 ＋ 0c 新增（预期 420 上下），**不得少於任一分支的總數**。
5. `aetheria_sim verify world-hash` 對 CLI 新建槽跑一次，通過。
6. `kSaveFormatVersion` 維持 22（0c 已 bump，本輪只確認**全 repo 恰好一處 22、零處 21 斷言殘留**）。

## 驗收（就這 4 條）

| # | 標準 |
|---|---|
| 1 | 三分支全併入，`git log --oneline` 可見三線 commit |
| 2 | 全套 ctest 綠，總數符合上式，附輸出尾段 |
| 3 | `grep -rn "kSaveFormatVersion\|== 21" core/ tests/` 佐證版本乾淨 |
| 4 | `ctest -R principle5` 綠（合併後重掃） |

## 不要做的事

| 不要 | 理由 |
|---|---|
| 改三線交付的實質內容（解衝突所需最小改動除外，逐條列進回報） | 內容已驗收 |
| bump 版本號 | v22 已定 |
| 動 wf/（除回報檔）、design/ | 歸檔調度者做 |
| push｜fan-out | 一律禁止 |

## 回報

`wf/inbox/m10-int-0-complete.md`（≤8KB 繁中）：第一行 DONE/BLOCKED/…；
衝突逐條（檔案、怎麼解）、測試總數、交互影響檢查結果。
中途 `.codex-inbox/m10-int-0.ask`（寫下假設繼續做）。
