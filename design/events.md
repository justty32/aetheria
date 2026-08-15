# 事件系統

> **本專案的第三根支柱**，與地圖的投影／歸約、生成的降維裁決並列——
> 而且三者是**同一條原則**在不同維度上的展開。
> 縮放的演算法見 [event-scaling.md](event-scaling.md)；交戰的具體例子見 [combat-scaling.md](combat-scaling.md)。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 統一原則

> **同層的物件之間永不直接溝通。一切互動透過共同的上層（或更低維的共用物）中介。**

這條原則在本專案已經出現三次，只是先前沒有點破：

| 場合 | 中介者 | 文件 |
|---|---|---|
| 兩個相鄰 Site 的邊界要對齊 | **邊與角**（更低維） | [edge-consistency.md](edge-consistency.md) |
| Site 的狀態要影響世界 | **Region**（更高層） | [interface-world-mid.md](interface-world-mid.md) |
| **Site A 要影響 Site B** | **Region**（更高層），透過事件 | 本文 |

好處是同一組：順序無關、缺席無關、重生成無關。
Site B 可能根本不在記憶體裡（`L_ABSENT`），但事件照樣被 Region 收下，
等 B 具現化時再投放。

## 事件是跨層物件

> **一個事件有一個權威狀態（住在它的「主場層」），以及各層各一個投影出來的面孔。**

這與 Region／Site 的關係完全同構——事件只是把同一套機制作用在**因果**上，
而地圖是作用在**空間**上。所以詞彙、測試、甚至實作都可以共用。

```
Region 面孔   兩個部隊圖標鎖定在一格上，每旬掉血
    ↕ 展開 / 聚合
Site 面孔     兩支部隊在戰場上機動，每小時結算陣型與士氣
    ↕ 展開 / 聚合
Local 面孔    敵方單位從地圖東邊緣走進來，玩家舉起劍
```

三個面孔描述的是**同一場戰鬥**。玩家看到哪一個，取決於他在哪裡看。

## 主場層是動態的

事件的權威狀態住在哪一層，由兩件事共同決定：

```
home = min( significance 所要求的最低層 , observer 允許的最高解析度層 )
```

| 情境 | 主場層 | 誰在算 |
|---|---|---|
| 玩家親身在場 | `Local` | 逐單位，戰鬥時每 6 秒一回合 |
| 玩家不在，但 mark 過該部隊 | `Site` | 逐部隊、逐小時（Total War 式） |
| 沒人看 | `Region` | 一條公式，逐旬 |

所以 **observer 不只決定「哪些地方要載入」，也決定「事件要算多細」**。
這是 [observer.md](observer.md) 的第二個作用，先前只寫了第一個。

**主場層會在事件進行中變動**：玩家騎馬趕到戰場，這場仗就從 Region 級的一條公式
升格成 Local 級的逐單位廝殺。升格與降格的演算法見 [event-scaling.md](event-scaling.md)。

## 資料形狀

```cpp
struct Event {
    EventId                  id;
    EventDefId               def;          // 資料檔定義，不是 enum
    Significance             significance; // 它最高能升到哪一層
    Level                    home;         // 當前主場層（動態）
    Tick                     started;
    Tick                     deadline;     // 0 = 無期限
    EventState               state;        // Pending / Active / Resolved
    std::vector<EntityRef>   participants; // 跨 zone 弱引用，解引用可失敗
    PropertyBag              props;        // 擴展欄位，整數化
};
```

- `def` 走 [definitions.md](definitions.md) 的規矩：事件種類是資料，不是 enum。
- `participants` 用 `EntityRef{zone, uid}` 弱引用，**解引用失敗是預期結果**
  （參與者可能已死、可能所在 zone 未載入）。這條直接繼承 medps
  `zone-addressing-lifecycle-design.md` §3.7。
- `props` 是整數化的稀疏屬性袋，讓擴展包能加自己的欄位。

## 事件信箱

Region tile 上掛一個**待投放事件佇列**：

```cpp
struct RegionTileInbox {
    std::vector<PendingEvent> queue;   // 依 Tick 排序
};
```

- Site A 想影響 Site B → 產生一個 Region 級事件 → Region 處理 → 投進 B 的信箱
- B 若是 `L_ABSENT`，事件就在信箱裡等著
- B 具現化或重載時，把積壓的事件**依時序**套用——這正好接上
  [interface-lifecycle.md](interface-lifecycle.md) 的補算流程，不必另發明機制

信箱的成長軸是「已造訪 tile 數」，符合 medps 的驗收不變量。
但仍要有上限：同一 tile 的信箱超過 N 筆就把舊的**合併成摘要**
（就像實體聚合成 Cohort，事件也可以聚合）。

## 事件的兩個方向

### 向下展開（decomposition）

高層事件 → 低層的具體表現。**上層決定下層的邊界條件**——
這句話在空間上是 `BoundaryProfile`，在因果上就是事件投放。

> Site 層說「部隊 B 從東側進攻」→ Local 層的邊界條件就是「敵方單位從東邊緣進入」。

展開必須是**確定的**：`seed = hash(event_id, target_zone_id)`。
同一個事件展開一百次，敵人永遠從同一個位置進來。

### 向上聚合（aggregation）

低層事件 → 高層的摘要。**必須有門檻**，否則 Region 會被 Local 的雞毛蒜皮淹沒。

| Local 事件 | 夠不夠格上升 | Site 面孔 | Region 面孔 |
|---|---|---|---|
| 玩家砍倒一個雜兵 | ✗ | 併入該部隊的傷亡統計 | — |
| 玩家殺了敵方指揮官 | ✓ | 該部隊士氣崩潰 | 該部隊潰散，戰局逆轉 |
| 玩家燒了糧倉 | ✓ | 圍城方補給中斷 | 該勢力的戰爭疲勞上升 |

門檻用的就是 [significance.md](significance.md) 的同一套等級——
事件與實體共用等級表，不另立一套。

## 與地圖機制的對應

三根支柱其實是一件事，這張表值得記住：

| | 空間（地圖） | 因果（事件） |
|---|---|---|
| 權威在高層 | Region 是唯一真相 | 事件的主場層持有權威狀態 |
| 向下展開 | 投影 `project()` | 展開 `decompose()` |
| 向上收攏 | 歸約 `reduce()` | 聚合 `aggregate()` |
| 上層給下層邊界條件 | `BoundaryProfile` | 事件投放點 |
| 橫向不直接溝通 | 邊界由邊與角裁決 | 跨 Site 影響由 Region 中介 |
| 不算兩次 | `has_live_site` 跳過 Region 公式 | 主場層以外的面孔**只讀不寫** |
| 一致性測試 | 卸載等價、無偏性 | 期望值一致、無偏性 |

**「主場層以外的面孔只讀不寫」** 是事件側的「不算兩次」規則：
一場仗的主場在 Site 時，Region 的面孔只是把 Site 的結果畫成兩個圖標，
Region **不得**自己再算一次損傷。

## 待細化

- `EventDef` 的完整欄位（展開規則、聚合門檻、期限行為）
- 信箱的合併策略與上限
- 事件之間的因果鏈（A 觸發 B）如何表達而不造成無限遞迴
- 事件的序列化：主場層狀態進存檔，其餘面孔重算
- **mark 機制**：使用者裁定先擱置，等縮放機制穩定後再回頭定，
  見 [significance.md](significance.md)
