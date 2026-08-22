# 任務書 M7-INT-5 — 併入 M7.3 Local 戰鬥，並讓它對真的 Site 戰鬥器校準

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**必讀協定**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)
**基準**：main（已含 M6.7 + M7.0 + M7.1 + M7.2）

---

## 兩件事，第二件才是重點

### 一、機械合併

`git merge --no-ff m7-3-wt` 在兩個 CMake 檔有衝突。
⚠ **括號陷阱（踩過五次）**：`target_sources(...)` 的開頭在衝突塊內、`)` 在塊外。
M7-INT-3／INT-4 已示範正確做法：**先補完前一塊的 `)`，再開新的獨立區塊。逐段解，不要批次。**

### 二、⚠ M7.3 是對「假的 Site」校準的，這輪要換成真的

M7.3 開工時 main 還沒有 `core/site/site_combat.*`，所以它照我授權的假設，
拿 M6.7 的 `resolve_scaled_combat(..., CombatLayer::Site, ...)` 當校準對象。
它自己在回報裡點名了：

> M7.2 真正 `site_combat` 尚未存在……**M7-INT-4 合併後應核對薄介面。**

**現在真的 Site 戰鬥器已經在 main 了。這輪要把 Local 的校準對象換成它，並重跑 N=1000。**

⚠ **校準順序不可顛倒**：Region 是基準 → Site 追 Region → Local 追 Site。
**Region 與 Site 的參數這輪都不准改**；只准調 Local。
若換成真 Site 後誤差超標而你必須調 Region 或 Site，**那是重大發現，寫進回報，不要自己改**。

## 那條符號斷言：確認它換對象後仍然有效

M7.3 把「不得全部同向」的分組從 `A／B／總計` 改成**三個 R bin 的 A／B 共六個值**。
理由成立（總計 = A + B，是算術上被決定的，三者同號在雜訊下幾乎必然發生），
而且我實測過改完仍抓得到真偏差。

**這輪要在真 Site 之下重做一次驗證**：
在 Local 的兩側損失各加 **+3% 同向偏差**，確認
`LocalCombat.ThousandBalancedSamplesTrackSiteWithMixedSignsAndHigherVariance` **變紅**，
並附六個 bin 值。做完還原。

## 驗收

| 判準 | 怎麼量 |
|---|---|
| 兩個衝突解掉 | `git diff --check` 過 |
| **Local 對真 Site 校準** | N=1000，`E[Local]` vs `E[真 Site]` 誤差**與六個 bin 的正負號** |
| 方差 Local > Site | 附兩個方差 |
| 薄介面是否要調 | 如實回報你改了什麼、為什麼 |
| 三層都在 | Region／Site／Local 的測試都綠 |
| 符號斷言有效 | 上述 +3% 注入的六個 bin 值 |
| 全套綠 | `ctest` 數字 |

⚠ **若換成真 Site 後誤差變大**，那是真實資訊，不是失敗——
M6.7 的「Region 加雜訊」本來就比真模擬容易對齊。**如實回報數字，不要調到好看。**

## 版本號

M7.3 沒有新增持久欄位，**維持 v20**，不要動。

## 不要做的事

改 Region 或 Site 的參數｜動版本號｜改 `design/`｜為了讓誤差好看而調參

## 回報

`wf/inbox/m7-int-5-merge-local-combat-complete.md`：兩個衝突怎麼解、
**對真 Site 的 N=1000 誤差與六個 bin 正負號**、兩個方差、
薄介面調整（若有）、+3% 注入的六個值、`ctest` 數字。

## 規約

- `cmake --build build --parallel 2`｜不准 fan-out 自我審查子 agent｜不要改 `design/`｜
  不要 push｜繁體中文、每份文件 ≤ 8 KB
