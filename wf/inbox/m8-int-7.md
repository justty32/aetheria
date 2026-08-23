# 任務書 M8-INT-7 — 併入 M8.2，且要把「基底比你以為的舊」這件事處理掉

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**必讀協定**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)
**工作分支**：直接在 `main` 上做這次合併（這是整合輪，不開 worktree）

---

## ⚠ 先講最容易出事的一件事

M8.2 的任務書寫「基準：M8.1 合併後的 main」，但實際 merge-base 是 `14eb84d`，
**那是 M8.0 併入之前**。所以：

- `m8-2-wt` 的 `cmake/targets_tests.cmake` **沒有 M8.0 的 Lua 沙箱測試區塊**
- M8.2 自己量到 `404/404`；`main` 上是 `412/412`

> **合併後的數字必須同時大於這兩個。** 若合併後 ctest 總數 ≤ 412，
> 代表你把某一邊的 target 弄丟了——**這種丟失不會有任何測試變紅**，只會安靜地少跑。

**驗收硬性要求**：回報裡要寫出
「合併前 main = 412、m8-2-wt = 404、合併後 = N」，且 **N ≥ 420**，並解釋 N 的組成。

## 兩處衝突，各有一個已知陷阱

### 1. `cmake/targets_tests.cmake`

兩邊都在**檔尾**追加區塊（M8.0 的 Lua 區 vs M8.2 的 runtime 區），
內容不重疊，**語意上兩邊都要留**。

⚠ **但不要盲目把兩邊貼在一起。** 這個檔案上一次就是這樣壞的：
衝突區把 `target_sources(...)` / `add_test(...)` 的括號切成兩半，
兩邊都留之後少一個 `)`，得到 `Parse error. Expected a command name`。

**做法**：先確認每個 `(` 都有配對的 `)` 再 commit。
`cmake -S . -B build` 能跑起來才算過。

### 2. `wf/workflows/common/code-map.md`

M8.2 這邊除了加 `runtime/` 那列，還**刪掉了一行**：

> `內部共用：gen_stage_ids.h、gen_grid.h、gen_noise.h、gen_hash.h；biome_classification.h 隔離 terrain／relief 裁決。`

⚠ 那一行描述的東西**沒有被刪除**，程式碼還在。合併時**把它留著**，
除非文件因此超過 8 KB——若超過，照鐵律拆檔（保留主檔名），不要靠刪內容瘦身。

## 合併後要自己驗的

| 項目 | 標準 |
|---|---|
| `cmake -S . -B build` | 通過（括號沒破） |
| `cmake --build build --parallel 2` | 通過，**且沒有沿用舊二進位** |
| `ctest --test-dir build --output-on-failure` | **N ≥ 420，全綠**，附 N |
| `./build/aetheria_sim --tick 62208000` | exit 0 |
| Godot headless 主場景 | exit 0 |
| M8.0 的 Lua 測試 | ⚠ **點名確認它們在合併後的清單裡**，附測試名 |
| M8.2 的 runtime 測試 | 同上，附測試名 |
| 每份文件 ≤ 8 KB | `find . -name '*.md' -size +8k` 為空 |

⚠ **陳舊二進位假綠**（已經咬過三次）：ninja 失敗但 ctest 跑舊執行檔會全綠。
**build 的 exit code 要自己確認是 0 才准跑 ctest。**

## 不要做的事

| 不要 | 理由 |
|---|---|
| 改 `design/` | 有設計異議寫進回報，我裁定 |
| 動 `kSaveFormatVersion` | 合併不該改存檔格式；維持 20 |
| 為了消衝突而刪掉任一邊的功能 | 兩邊都要在 |
| push | 一律要使用者點頭 |
| fan-out 自我審查子 agent | 禁止 |

## 回報

`wf/inbox/m8-int-7-complete.md`：三個 ctest 數字與 N 的組成、
兩處衝突各自怎麼判的、M8.0 與 M8.2 測試名點名、
檔案大小檢查結果、現有測試證不了的事。

## 規約

- `cmake --build build --parallel 2`｜不准 fan-out 自我審查子 agent｜不要改 `design/`｜
  不要 push｜繁體中文、每份文件 ≤ 8 KB
