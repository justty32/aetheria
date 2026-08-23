# 任務書 M8.3 — 建築型別映射：蓋住宅不該等於蓋領主廳

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**必讀協定**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)
**工作分支**：`m8-3-wt`（新 worktree，基準 = M8-INT-7 合併後的 main）
**設計依據**：[interface-world-mid.md](../../design/interface-world-mid.md) 的投影／歸約與歸約量表、
[interface-double-count.md](../../design/interface-double-count.md)

---

## 我的裁定（回覆你 M8.2 回報末段那條）

你問的是：M3 `CityBuilding` 與 M2 `PersistentBuilding` 沒有正式型別映射，
而 Development reduction 只吃 `SettlementHall`，所以你在住宅完成後補一個
`SettlementHall` projection marker 走正式 ReductionTable。

**裁定：本輪的權宜我接受，不擋合併——但它是錯的，這一輪把它修掉。**

理由：那個 marker 讓**蓋住宅順帶帶來領主廳的人口貢獻**。
這不是「數值偏一點」，是**一種建築被當成另一種**。
`interface-world-mid.md` 的歸約量表要求每個上報量有明確來源；
一個假冒的 marker 讓來源說不清，而且它**不會有任何測試變紅**——
這正是 [verification-detection-power.md](../../design/verification-detection-power.md)
說的那種假通過。

⚠ 而且它會咬到 M9.0：主線的推進條件要掛在城建上，映射錯了主線的數字也跟著錯。
所以這輪排在 M9.0 之前。

## 要做的

1. **`CityBuilding` → `PersistentBuilding` 的正式型別映射**，取代 projection marker。
2. **不同 CityBuilding 對 Region 有不同權重**：住宅不等於領主廳。
   權重表走既有 `ReductionTable`，不要另建一條通道。
3. 移除 M8.2 那個 `SettlementHall` projection marker，並確認移除後
   住宅**不再**帶來領主廳的人口貢獻。

⚠ **不要順手擴大成「城建系統重做」。** 只做映射與權重，既有城建玩法一個字不動。

## ⚠ 存檔版本號：這一輪獨佔 21

映射要進存檔，所以 `kSaveFormatVersion` **20 → 21，只有這一路能動**。
M9.0 之後才輪到 22。（OPS-NOTES 記過：沒劃清楚，一晚撞了兩次版本號。）

⚠ **缺席 ≠ 中性**（[zone-save-history.md](../../design/zone-save-history.md)）：
舊存檔沒有映射欄位時**不要默默當成住宅**，照既有慣例 fail-fast。

## 順手一件（跨區，只有你能做）

`wf/SESSION-LOG.md` 的 **`### 實作者（gpt-sol）`** 區塊裡，M8.2 那條已經完成並併入 main，
**請你刪掉它**（完成即整條刪除，不留已完成清單）。那塊歸你擁有，我不碰——
這是規約 [CONTACTS.md](../workflows/inbox/CONTACTS.md) 的「同步規約」。
若這輪還在跑，就換成 M8.3 的 open 條目。

## 驗收（每一條都要在回報裡附數字）

| 項目 | 標準 |
|---|---|
| **住宅與領主廳的 Region 效果不同** | 各蓋一棟，附兩者對 Development 與人口的**前後數字**，且**必須不同** |
| **marker 已死** | 全 repo grep 不到那個權宜 marker，附 grep 結果 |
| **移除後住宅不再帶人口** | 附移除前／移除後的人口數字 |
| **不重複計算** | 依 [interface-double-count.md](../../design/interface-double-count.md)，附一次事件升級與歸約同時發生的比對 |
| M8.2 的迴圈沒壞 | M8.2 那條「Site 蓋住宅 → Region 建設 1→2」照樣通，附數字（值可以變，鏈路不能斷） |
| 存檔往返 | 冷載入（新行程）後映射與權重逐項相等 |
| ctest | 全綠，附 N 與新增數 |
| 文件 | `find . -name '*.md' -size +8k` 為空 |

### 負向控制（一次只改一條，取得紅燈後還原）

| # | 注入 | 必須紅 |
|---|---|---|
| 1 | 把所有 CityBuilding 的權重設成相同 | 「住宅與領主廳效果不同」那條 |
| 2 | 把權宜 marker 加回去 | 「移除後住宅不再帶人口」那條 |
| 3 | 讓歸約與事件升級都上報同一筆 | 不重複計算那條 |

⚠ **無效注入不算通過**：若某條注入下來全綠，**如實寫進回報說這條測不到**，
不要換一個更容易紅的注入充數。

## 不要做的事

| 不要 | 理由 |
|---|---|
| 改 `design/` | 有設計異議寫進回報，我裁定 |
| 重做城建玩法 | 這輪只做映射與權重 |
| 為權重反覆試值到「數字好看」 | 一輪一個變因；試了幾個值如實寫進回報 |
| 碰 `godot/`、`tools/` | 可能有別輪並行 |
| push | 一律要使用者點頭 |
| fan-out 自我審查子 agent | 禁止 |

## 回報

`wf/inbox/m8-3-building-type-mapping-complete.md`：
映射表與權重表、住宅 vs 領主廳的前後數字、marker 的 grep 結果、
移除前後的人口數字、不重複計算的比對、M8.2 迴圈仍通的數字、
冷載入往返、三條負向控制各自的紅燈（或哪一條測不到）、
`kSaveFormatVersion` 21 的欄位、ctest N 的組成、現有測試證不了的事。

## 規約

- `cmake --build build --parallel 2`｜**build exit code 是 0 才准跑 ctest**｜
  不准 fan-out 子 agent｜不改 `design/`｜不 push｜繁體中文、每份文件 ≤ 8 KB
- 有疑問但猜得下去 → `.codex-inbox/m8-3.ask`：**寫下假設然後照假設做完，不要停**。
  真的做不下去 → `.codex-inbox/m8-3.blocked` 並停。完成 → `.codex-inbox/m8-3.done`。

## 最後一條，最重要

**如果你發現某個權重差異其實在遊戲裡看不出來、某條負向控制測不到、
或這個映射其實可以被更簡單的東西取代，如實回報，不要硬湊一個情境糊過去。
這比做完更有價值。**
