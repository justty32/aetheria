# 任務書 M5-pre — 批次推進與「旬界只結算 Region 一次」

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**必讀設計**：[`interface-world-mid.md`](../../design/interface-world-mid.md)、
[`interface-lifecycle.md`](../../design/interface-lifecycle.md)、
[`principles.md`](../../design/principles.md) 原則七
**基準**：`171beb1`
**與 M5.0 並行**：另一個 worktree 在做 `core/local/`。**這輪不要新增檔案**
（避免兩邊同時改 `CMakeLists.txt`），只改既有的。

---

## 這是你自己在 M3.4 標出來的架構風險

我當時記進了 `SESSION-LOG`：

> `SiteTurnPipeline` 目前一次只推進一個 `L_FULL` Site。M5 的世界時鐘／串流協調器
> 必須**批次推進多個 Site**，且**保證旬界只結算 Region 一次**。

M5 會讓這件事從「未來風險」變成「立刻爆炸」——[`lowmap-streaming.md`](../../design/lowmap-streaming.md)
的串流是 **3×3 全載**，而 [`interface-lifecycle.md`](../../design/interface-lifecycle.md) 明寫
**「同時可以有很多個 `L_FULL` 的 Site」**。所以 M5.0 之前先把這條修掉。

### 缺陷的確切形狀

`core/site/site_build_loop.h:145`

```cpp
SiteAdvanceReport advance_hours(zone::Zone& site, zone::Zone& region, ...) const;
private:
    world::RegionTurnPipeline region_turn_;
```

每個 Site 各自在自己的 240 小時邊界呼叫 `region_turn_`。
**兩個 L_FULL Site 各推進 240 小時 → Region 被結算兩次。**
這正是 M2.5 那條「不算兩次」規則在多 Site 情境下的延伸，
只是這次重複的不是事件，是**整個 Region 回合**。

---

## 要做的

1. **批次入口**：一次接收一組 `L_FULL` Site，推進 `hours`。
   單 Site 是它的特例，不要留兩條會分岔的路徑。
2. **旬界只結算 Region 一次**：所有 Site 推進到旬界 → 各自把歸約寫回 Region →
   **`RegionTurnPipeline` 跑一次** → 再繼續。
   ⚠ 順序很重要：**先收齊全部歸約，再結算**。若邊推進邊結算，
   後面的 Site 看到的 Region 就跟前面的不一樣了。
3. **推進順序正規化**：依 `ZoneKey` 排序，不依呼叫者傳入的順序。
4. **降級／逐出仍只在回合結算尾端執行**（原則七），批次化不得鬆動這條。

## 不要做

串流協調器本身、場強重算、載入／卸載策略、預生成。
**這輪只修「多個 Site 同時推進時的結算語意」**，不是寫 M5 的世界時鐘。

---

## 驗收

| 判準 | 怎麼量 |
|---|---|
| **旬界 Region 結算次數 == 1** | N 個 Site 各推進 240 小時，計數必須是 1，不是 N |
| **⚠ 負向控制：舊路徑要被抓出來** | 見下節。**這條比上面那條重要** |
| **順序無關** | 同一組 Site 打亂傳入順序 → **正規化狀態雜湊相同** |
| **等價於單 Site** | 只有一個 Site 時，批次路徑與 M4 的既有結果**逐項相同**（202 個測試不准變） |
| **算術可分辨** | 見下節 |

### ⚠ 負向控制：測試要能看見「結算兩次」

M2.5 那輪你做得很漂亮的地方是**算術本身能分辨單算與雙算**（`200→175→150`，
而雙算會是 `125`）。這輪照做：

> **刻意讓兩個 Site 各自呼叫舊的單 Site 路徑，測試必須失敗，
> 而且失敗的數字要能認出「這是被算了兩次」，不只是「不相等」。**

挑一個**雙算與單算數值必然不同**的量（人口成長、糧食產出都可以）。
⚠ 不要挑一個效果為 0 的量來充數——那樣兩條路徑會給出相同答案，測試沒有偵測力。

### 量測規約

效能斷言一律**暖機後取 N≥5 次最小值**；**禁止重試到過為止**。
每個量測要附「路徑真的跑過」的計數（推進了幾個 Site、幾次旬界、幾次歸約寫回）。
**計數為 0 的量測不算通過**——今天三次假通過都是這樣來的。

---

## 回報

寫進 `wf/inbox/`，檔名 `m5-pre-batch-advance-complete.md`：

1. 驗收表逐條**實測數字**
2. **負向控制**：舊路徑跑出來的錯誤數字是多少、為什麼那個數字能認出是雙算
3. 存檔位元流有沒有變（有就要升版，照 `zone-save-format.md`；
   **版本沿革表由你維護**，那是紀錄不是設計決策）
4. 你認為現有測試證不了的事

## 規約

- 建置 `cmake --build ... --parallel 2`（**不帶數字會開滿核心**）
- **不准 fan-out 自我審查子 agent**
- **不要新增檔案**（M5.0 在並行，避免 `CMakeLists.txt` 衝突）
- 繁中、≤ 8 KB、`aetheria_core` 零 godot-cpp 依賴
- commit 到 `main` 照常；**push 一律先問**
- **不要改 `design/`**
