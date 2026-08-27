# 任務書 M10.0b — 寫死的世界雜湊常數退場，改關係斷言

**寄件人**：規劃者（調度 session）
**收件人**：gpt-sol 實作者
**必讀**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)、[計畫](../workflows/plan/runtime-injection-waves.md) M10.0b 節
**工作分支**：`m10-0b`（worktree `../aetheria-wt-m10-0b`）

## 背景

M10 後面每一波都要 bump 存檔版本。tests/ 裡的 golden 雜湊常數（KNOWN-TRAPS 早記著
「是 golden snapshot、不是領域不變量」）會讓每次 bump 都變成一場常數重寫。
**這輪先把它們換成關係斷言，之後格式再怎麼變都不用回來改。**

調查量到的基數：引用世界雜湊的測試檔 21 個、寫死的 64-bit 常數 14 個
（`region_determinism_test.cpp` 占 6 個）。⚠ **不是每個常數都是 golden**：
像 `14695981039346656037` 是 FNV offset basis（演算法常數，合法），要逐條分辨。

## 要做的

1. **先盤點**：列出全部 14 個常數（檔案:行號），逐條分類：
   - `golden`：某次執行的世界/區域雜湊快照 → 要退場
   - `algorithm`：FNV basis、splitmix 常數之類 → 保留，加註解說明為什麼合法
   - 拿不準的 → 列出來，寫上你的判斷與理由，照判斷做
2. golden 類改寫成**關係斷言**，用這四種形式（挑對的用，不必全上）：
   同一執行內「存→讀」雜湊相等；正序/反序建構雜湊相等；冷載入（重開 store）雜湊相等；
   同參數跑兩次雜湊相等。**斷言的兩邊必須是同一次測試裡各自算出來的**，
   不准一邊又是寫死常數。
3. 改寫時保留原測試的偵測意圖：原本抓「schema 漂移」的，改完要仍抓得到
   「存讀不一致」——不是把斷言刪掉。

## 驗收（就這 4 條）

| # | 標準 |
|---|---|
| 1 | 分類表：14 條全列（檔案:行號、golden/algorithm/拿不準、處置） |
| 2 | `grep -rn` 證明：tests/ 內不再有未加註解的 golden 世界雜湊常數 |
| 3 | 負向控制：臨時在 `core/serialize/zone_encode.cpp` 翻一個 byte（如某欄位 +1）→ 至少一條改寫後的關係斷言**紅**，附輸出；還原後全綠 |
| 4 | 全套 `ctest` 綠（`--parallel 2`），測試總數**不減** |

## 模組與依賴（原則九，回報必填）

預期零新模組；若你需要共用的「建世界算雜湊」helper，放 `tests/support/`，只被 tests 依賴。

## 不要做的事

| 不要 | 理由 |
|---|---|
| 動 `core/`、`bridge/`、`godot/`（負向控制的臨時翻 byte 除外，必還原） | 領地只有 `tests/` |
| 新增測試 `.cpp` 檔或動 CMakeLists | 只改既有檔；真需要新檔→寫進回報讓整合輪加 |
| 碰 `principle5` 相關新檔 | M10.0a 正在並行建它們 |
| 動 `kSaveFormatVersion` | M10.0c 獨佔 |
| 為了讓某條關係斷言通過而放寬容差或改 core 行為 | 斷言遷就現實，不是現實遷就斷言；改不動就如實回報 |
| build 超過 `-j2`、ctest 超過 `--parallel 2` | 同機另有兩路在編譯 |
| push｜fan-out 子 agent｜改 `design/` | 一律禁止 |

## 回報

`wf/inbox/m10-0b-relational-hashes-complete.md`（≤8KB，繁中）：
第一行標 `DONE`/`BLOCKED`/`NEEDS-USER`/`FAILED`；分類表、負向控制前後輸出、
grep 證明、模組與依賴節。中途疑問 `.codex-inbox/m10-0b.ask`（寫下假設繼續做）。

**最重要**：如果你發現某條測試把 golden 常數拿掉之後**什麼都驗不了**
（它唯一的價值就是那個快照），如實回報「這條其實沒有關係式可寫」，
不要硬湊一個恆真的斷言。找出這種測試比改寫它更有價值。
