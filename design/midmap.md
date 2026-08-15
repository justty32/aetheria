# L2 中層地圖（Site）

> Region 的一格放大後的樣子。一小時一回合。玩法是城建經營或 SRPG 戰略。
> 上層見 [outline.md](outline.md)；與大地圖的界面見 [interface-world-mid.md](interface-world-mid.md)。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 網格

- **Square 四鄰接**，64×64 = 4096 格，一格 125 m（一個街廓／一片林地的尺度）。
- 座標 `struct SiteXY { uint8_t x, y; };`——64×64 塞得進 8 bit，整個座標一個 `uint16_t`。
- Site 恆等於 Region 的**恰好一格**，沒有跨格的 Site。要表現跨格的大城，
  用「主 Site + 鄰格衛星 Site」在 Region 層做關聯，不是把 Site 放大。

## Site 種類

種類由 Region tile 的狀態決定（見界面文件的投影規則），不是獨立設定：

| 種類 | 觸發條件（Region tile） | 玩法 |
|---|---|---|
| `City` | 有城市建設，人口 ≥ 門檻 | 城建經營為主，受攻時轉 SRPG |
| `Town` | 有聚落但規模小 | 輕量經營 + 交易 |
| `Fortress` | 有軍事建設 | SRPG 為主，駐軍與防禦工事 |
| `Wilderness` | 無建設的陸地 | 通過、採集、遭遇、隨機遺跡 |
| `Ruin` | feature = 廢墟 | 探索為主，通往 L3 的入口密度高 |
| `Sea` | 海格 | 航行、海戰、海上遭遇 |

同一種類共用一套生成器與一套回合規則；種類是**參數化的**，不是六份獨立實作。

## 資料模型

同樣是 SoA，但比 Region 多一層「物件」：

```cpp
struct SiteTiles {                       // 4096 格的平行陣列
    std::vector<GroundId>  ground;       // → GroundDef 下標（土/草/石板/水/牆基…）
    std::vector<OverlayId> overlay;      // → OverlayDef 下標（道路/田地/廣場/瓦礫…）
    std::vector<EntityId>  structure;    // 建築實例，0 = 無
    std::vector<EdgeId>    edges;        // 每格四條邊：牆、門、柵欄、河岸
    std::vector<ZoningId>  zoning;       // → ZoningDef 下標（住宅/商業/工坊/軍事/農地/野外）
};

struct SiteObjects {                     // 稀疏：建築、單位、道具、L3 入口
    std::vector<Building> buildings;
    std::vector<Actor>    actors;
    std::vector<Portal>   portals;       // 通往 L3 Local 的入口
};
```

**牆是邊屬性**，和 Region 的河流道路共用同一套 `EdgeDef` 機制（見 [definitions.md](definitions.md)）：
牆長在兩格之間，門就是「某條邊上帶可開關旗標的 def」。
視線與尋路都只需查邊，不必發明「半格」概念，牆／柵欄／閘門／護城河的種類也無上限。

種類欄位一律是**資料檔定義的下標，不是 enum**——連 Site 種類本身（下表）也是 `ZoneKindDef`。

## 回合：一小時

平時一小時一回合（一旬 = 240 回合）；**交戰時 stride 縮到 15 分鐘**（一旬 = 960 回合），
見 [outline.md](outline.md)。回合階段：

1. 玩家指令（建造、下令、移動單位）
2. 命令執行
3. 單位行動（AI 與中立生物）
4. 生產結算（工坊產出、農地收成按季節、建造進度 +1 小時）
5. 事件判定
6. 時鐘 += 3600 秒（交戰時為 900 秒）；若跨過旬界，**先把 Region 回合結算跑完再繼續**

第 6 點是三層時間統一的落地：Site 不會偷偷停住世界。玩家在城裡經營一整旬，
外面的戰爭與季節照樣推進。

## 玩法循環 A：城建經營（City / Town）

- **分區與街廓**：城市由分區組成（住宅、商業、工坊、軍事、農地）。玩家在格子上劃分區、蓋建築。
- **建築**：佔 1～4 格，有建造時數（以小時計）、維護成本、產出、相鄰加成。
- **相鄰加成**是主要的空間趣味：工坊挨著礦場 +產出，住宅挨著工坊 −滿意度，
  廣場給周圍住宅 +滿意度。這讓「怎麼擺」有意義，而不只是「蓋什麼」。
- **人口**：住宅提供容量，糧食與滿意度決定成長。人口是 Site ↔ Region 的主要歸約量之一。
- **產出上繳**：Site 的產出每旬歸約成 Region 層的一筆數字，不逐小時同步。

## 玩法循環 B：SRPG 戰略（Fortress / 受攻的 City）

- **部隊來自 Region**：進攻方與防守方的編成由 Region 層的部隊投影而來。
- **戰棋**：四鄰接移動、地形加成（丘陵 +射程、林地 +迴避、城牆 +防禦）、
  面向與側背、行動點制。
- **建築就是地形**：城建蓋出來的牆、塔、閘門直接成為戰場地形。
  這是兩套玩法真正咬合的地方——你怎麼蓋城，決定你怎麼守城。
- **結果歸約回 Region**：勝敗、雙方傷亡、城市損毀程度、控制權變更。

## 玩法循環 C：荒野（Wilderness / Ruin / Sea）

- 主要是**通過**：部隊從 Region 移動經過此格時，可選擇進入細看或直接掠過。
- 採集點、遭遇、隨機小遺跡；遺跡格提供通往 L3 的入口。
- 荒野 Site 幾乎沒有持久層，卸載成本極低，是生命週期機制最常運作的地方。

## 生命週期（概觀）

Site 不是永久存在的，也不是「一次只有一個活著」。

哪些 Site 活著、用什麼解析度活著，由**觀察點機制**決定（[observer.md](observer.md)）：
玩家是一個 observer，玩家 mark 過的人事時地物、進行中的大事件、高重要性實體
也各自是 observer。每個 observer 向**同層鄰域**與**上下層**輻射，
結果是四個解析度等級之一：

| 等級 | 這個 Site 在做什麼 |
|---|---|
| `L_FULL` | 逐小時回合模擬，個別實體都算 |
| `L_COARSE` | 只跑聚合統計，個體併進團體 |
| `L_FROZEN` | 不跑，被查詢時才即時投影 |
| `L_ABSENT` | 不在記憶體 |

所以玩家所在的 Site 是 `L_FULL`，鄰近幾個 Site 是 `L_COARSE`（商隊會來、敵軍會逼近，
但不逐格逐人算），遠方且無人在意的 Site 是 `L_ABSENT`。
玩家 mark 過的遠方城市即使人不在，也會被拉到 `L_COARSE` 以上。

至於 Site 裡的**哪些實體**要個別計算，是另一條軸——見 [significance.md](significance.md)。

完整的具現化／卸載／補算規則，以及它為什麼不會讓世界狀態漂移，
在 [interface-world-mid.md](interface-world-mid.md) 與 [interface-lifecycle.md](interface-lifecycle.md)。

## 生成流程

| Site 種類 | 文件 |
|---|---|
| `City` / `Town` / `Fortress` | [sitegen-city.md](sitegen-city.md) — 骨架（主幹道→街廓）先行，填充（分區→建築→城牆）在後 |
| `Wilderness` / `Ruin` / `Sea` | [sitegen-wild.md](sitegen-wild.md) — 更便宜、持久層極小，是 LOD 機制最常運作的地方 |

**接邊一致性**是這兩者共同的前提：相鄰 Site 的河流、道路、海岸線必須嚴絲合縫。
作法是**邊界由更低維的物件裁決**（角 → 邊 → 面），Site 永遠不寫自己的邊界，
邊界是它的輸入。完整機制與四項驗證測試見 [edge-consistency.md](edge-consistency.md)。

生成預算 **< 30 毫秒**（玩家進城的當下生成，不能卡），
因此禁止任何迭代式或回溯式演算法——見 [gen-pipeline.md](gen-pipeline.md)。

## 待細化

- 建築表、分區規則、相鄰加成的完整數值
- SRPG 的行動點與陣型規則（戰鬥的期望值基準已定，見 [combat-formula.md](combat-formula.md)）
- Site 種類擴充（礦坑、港口、修道院……）——加一筆 `ZoneKindDef` 即可，不必改程式
