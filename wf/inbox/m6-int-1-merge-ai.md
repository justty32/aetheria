# 任務書 M6-INT-1 — 把 M6.5 併進 main：解掉 v16 版本號碰撞

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**必讀設計**：[`zone-save-history.md`](../../design/zone-save-history.md)、
[`zone-save-format.md`](../../design/zone-save-format.md)
**基準**：main（`m6-5-wt` 尚未併入）

---

## 這輪是整合，不是新功能

`m6-5-wt`（勢力 AI）要併進 main。`git merge --no-ff m6-5-wt` 會產生三個衝突，
**其中一個不是機械衝突，需要判斷**。

## ⚠ 真正的問題：兩路都升到 v16，但那是兩份不同的 payload

| 分支 | 宣稱版本 | 實際加了什麼 |
|---|---|---|
| main（M6.4 已併入） | v16 | `NamedFateLedger` |
| `m6-5-wt` | v16 | `FactionTruth`、`KnowledgeRecord`、`FactionMindState` |

兩路並行時各自看到的上一版都是 v15，所以各自升 v16——**碰撞是我派工造成的，不是你們的錯**。

**裁定：合併後的格式是 v17，內含兩者。** 理由：v16 從來沒有一個穩定的定義，
兩個「v16」在磁碟上是不同的位元流，讓任何一個沿用 v16 都會製造一個**讀得進去但錯位**
的格式——那正是 [`zone-save-format.md`](../../design/zone-save-format.md) 說
「版本欄位把靜默讀壞變成大聲拒讀」要防的事。

⚠ 所以：**不要保留任何「v16」的解碼路徑**。v15 → 拒讀（照既有政策不做遷移），
v17 → 現行。中間那個從未存在過的 v16 不要留相容分支。

## 三個衝突

| 檔案 | 性質 | 怎麼解 |
|---|---|---|
| `cmake/targets_core.cmake` | 純加法（兩側都是檔尾追加區） | 兩邊都留 |
| `cmake/targets_tests.cmake` | 純加法 | 兩邊都留 |
| `tests/zone/diplomacy_save_test.cpp` | **兩個都有效的 v15 相容測試** | **兩個都要留**，各自改名並改成驗 v17 |

⚠ 第三個特別注意：`DiplomacySave.V15RootLoadsWithLegacyComponentsAndDiplomacyBlock`
（main 側，驗 `NamedFateLedger` 缺席）與
`DiplomacySave.V15KeepsNewKnowledgeAndMindFieldsAbsent`
（M6.5 側，驗 knowledge／mind 缺席）**驗的是不同欄位，兩個都有意義**。
合併後應該有一個測試同時斷言**四類新欄位全部缺席**，或兩個測試各管一半——你選，但
**四類（`NamedFateLedger`／`FactionTruth`／`KnowledgeRecord`／`FactionMindState`）
一個都不能漏**。

⚠ **「缺席」不是「預設值」。** 這條 M6.2b 與 M6.4 都做對了，合併後不要退化。

## 合併時的括號陷阱

前幾輪踩過：衝突區會**把函式主體或 `target_sources(...)` 的括號切開**——
`{` 或 `(` 在衝突塊內、結尾在塊外。用「兩邊都取」的通則批次解會**漏掉結尾括號**，
而編譯器報的錯離病灶很遠（實測看到的是命名空間套疊，完全指不到出事的檔）。
**逐個看清楚再解。**

## 驗收

| 判準 | 怎麼量 |
|---|---|
| 合併完成 | 三個衝突都解掉，`git diff --check` 過 |
| 版本是 v17 | 沒有殘留的 v16 解碼路徑，`grep` 得出來 |
| v15 拒讀 | 大聲拒讀，不是讀成預設值，附證據 |
| 四類新欄位全驗到缺席 | 附四個斷言的位置 |
| 全套綠 | `ctest` 全數通過，附數字 |
| 雜湊 | 冷往返正規化雜湊相同，附前後值 |

**負向控制**：把四類新欄位中**任選一類**從「v15 應缺席」的斷言拿掉，
確認那個測試對該類的漏失**再也抓不到**——證明四個斷言各自有效，不是互相頂替。
附紅／不紅的實測。

## 不要做的事

| 不要 | 理由 |
|---|---|
| 沿用 v16 | 兩個不同的 v16 會製造靜默讀壞 |
| 保留 v16 相容分支 | 那個版本從未穩定存在過 |
| 做 v15 → v17 遷移 | 政策是拒讀，不做遷移 |
| 動 `core/ai/` 的演算法 | 這輪只做整合，不改行為 |
| 改 `design/`（v17 沿革寫進回報，我來加） | 我裁定 |

## 回報

`wf/inbox/m6-int-1-merge-ai-complete.md`：三個衝突各自怎麼解、
v15 拒讀的證據、四類缺席斷言的位置、負向控制實測、
`ctest` 數字、現有測試證不了的事。

## 規約

- `cmake --build build --parallel 2`｜不准 fan-out 自我審查子 agent｜不要改 `design/`｜
  不要 push｜繁體中文、每份文件 ≤ 8 KB
