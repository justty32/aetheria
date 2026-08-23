# 任務書 M9-INT-8 — 併入 M8.3 與 M9.2／M9.2b

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**必讀協定**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)
**工作分支**：直接在 `main` 上做（整合輪，不開 worktree）
**要併的兩條**：`m8-3-wt`（建築型別映射）、`m9-2-wt`（美術管線 + 偵測力補強）

---

## ⚠ 這次的門檻是推導出來的，不是估的——請照著檢查我有沒有推錯

上一個整合輪（M8-INT-7）我寫「N ≥ 420」，那是**拍腦袋估的**，實際聯集是 414，
害你得先寫 `.ask` 才沒去湊數。這次改成關係式：

```
合併後 ctest N  =  417  =  414（main 基準）+ 3（M8.3 新增）
合併後 pytest   =  18       （m9-2-wt，不掛進 ctest）
```

那 3 條我已經自己在 `m8-3-wt` 上列過，就是：

- `#397 SiteBuildingMapping.EveryCityBuildingDefHasTheExpectedPersistentTypeAndWeight`
- `#398 SiteBuildingMapping.HouseAndSettlementHallHaveDifferentObservableRegionEffects`
- `#399 SiteBuildingMapping.ColdFileLoadDerivesTheSameTypeAndWeightFromDefinitionId`

**回報要寫出這兩個數字與 N 的分解。**
⚠ 若實際 N 對不上，**不要調整任何東西去湊**——把實際數字與分解如實寫出來，
並指出是我推錯還是真的掉了 target。**對不上本身就是有價值的資訊。**

⚠ 真正要防的還是那條：**「少跑某一邊的 target」不會有任何測試變紅，只會安靜地少跑。**
所以請**點名確認**下列三組在合併後的 ctest／pytest 清單裡：
M8.0 的 Lua 10 條、M8.2 的 runtime 2 條、M8.3 新增的那幾條。

## 兩條分支的檔案交集

`m9-2-wt` 只動 `tools/art/` 與自己的兩份收件匣信；`m8-3-wt` 動 `core/` 與
`wf/SESSION-LOG.md`。**理論上沒有交集**，但 `main` 這邊我動過
`wf/SESSION-LOG.md`、`wf/OPS-NOTES.md`、`wf/INDEX.md`，新增了 `wf/OPS-VERIFY.md`。

⚠ 所以最可能衝突的是 **`wf/SESSION-LOG.md`**：你在 `m8-3-wt` 改了**實作者區塊**，
我在 `main` 改了**規劃者區塊**與檔尾的操作心得連結。
**兩邊都要留**——這是分區塊擁有，不是二選一。
⚠ 那個檔**貼近 8 KB 上限**，合併後若超過就照鐵律拆檔（保留主檔名），不要靠刪內容瘦身。

⚠ 合併衝突的老坑（[OPS-NOTES](../OPS-NOTES.md)）：衝突區會把 `target_sources(...)` /
`add_test(...)` 的括號切成一半。**commit 前確認每個 `(` 都有配對的 `)`**，
`cmake -S . -B build` 能跑起來才算過。

## 合併後要自己驗的

| 項目 | 標準 |
|---|---|
| `cmake -S . -B build` | 通過 |
| `cmake --build build --parallel 2` | **exit code 自己確認是 0 才准跑 ctest** |
| `ctest --test-dir build --output-on-failure` | 全綠，附 N 與分解 |
| `python3 -m pytest -p no:cacheprovider tools/art/tests -q` | 18 綠 |
| `python3 tools/art/verify_acceptance.py` | exit 0 |
| `./build/aetheria_sim --tick 62208000` | exit 0 |
| Godot headless 主場景 | exit 0 |
| `kSaveFormatVersion` | **21**（M8.3 帶進來的），不要再動 |
| 三組測試點名 | 見上，附測試名 |
| 文件 ≤ 8 KB | `find . -name '*.md' -size +8k` 為空 |

⚠ **陳舊二進位假綠**（已咬過三次）：ninja 失敗但 ctest 跑舊執行檔會全綠。

## 順手一件（我驗 M9.2b 時發現的，不值得單開一輪）

`tools/art/verify_acceptance.py` 的故障注入是**對原始碼做字面字串取代**，
所以綁死在 `load_palette` 那一行的寫法上。我外部改了那一行之後，
注入器找不到目標而拋 `RuntimeError`。

**失敗方向是對的（fails closed，不會假綠），所以不急**——
但錯誤訊息會誤導成「注入器壞了」而不是「工具被改了」。
請在那個 `RuntimeError` 的訊息裡加一句提示：**找不到目標多半代表被注入的那行已被修改，
注入器要跟著更新**。只改訊息，不要重寫注入機制。

## 不要做的事

| 不要 | 理由 |
|---|---|
| 改 `design/` | 有設計異議寫進回報，我裁定 |
| 為了讓 N 好看而增刪測試 | 見上，對不上要如實寫 |
| 為了消衝突刪掉任一邊的 SESSION-LOG 區塊 | 分區塊擁有，兩邊都要在 |
| 重寫 `verify_acceptance.py` 的注入機制 | 只改錯誤訊息 |
| push | 一律要使用者點頭 |
| fan-out 自我審查子 agent | 禁止 |

## 回報

`wf/inbox/m9-int-8-complete.md`：三個數字與 N 的分解（含對不上時的說明）、
衝突各自怎麼判的、三組測試名點名、`kSaveFormatVersion` 確認、
檔案大小檢查、現有測試證不了的事。

## 規約

- `cmake --build build --parallel 2`｜**build exit code 是 0 才准跑 ctest**｜
  不准 fan-out 子 agent｜不改 `design/`｜不 push｜繁體中文、每份文件 ≤ 8 KB
- `.codex-inbox/m9-int-8.{progress,ask,blocked,done}`；`.ask` **寫下假設繼續做，不要停**。

## 最後一條，最重要

**如果 N 對不上、或某一組測試你其實找不到、或某項驗收你其實沒真的做到，
如實回報，不要硬湊一個情境糊過去。這比做完更有價值——
上一輪就是靠你這樣做，才擋下我一個錯的門檻。**
