# L1 大地圖（Region）

> 一個大陸或小世界。文明系列玩法，一旬一回合。
> 上層見 [outline.md](outline.md)；與中層的界面見 [interface-world-mid.md](interface-world-mid.md)。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 網格與座標

- **Square 四鄰接**（上下左右）。沒有對角移動，也就沒有 √2 成本與轉角穿牆的例外處理。
- 尺寸 128×96 = 12288 格，一格 8 km（約 1000 km × 770 km，接近伊比利半島規模）。
- 座標 `struct RegionXY { int16_t x, y; };`，線性索引 `idx = y * width + x`。
  三層的座標型別分別是 `RegionXY`／`SiteXY`／`LocalXY`，見 [glossary.md](glossary.md)。
- 邊界不環繞。世界的延續由 [WorldGraph](#世界圖多個大地圖之間) 承擔，不靠地圖捲繞。

## Tile 資料佈局：SoA

全圖狀態是**平行陣列（structure of arrays）**，不是 `std::vector<Tile>`：

```cpp
struct RegionTiles {
    uint32_t w, h;
    std::vector<TerrainId> base;       // → TerrainDef 下標
    std::vector<ReliefId>  relief;     // → ReliefDef 下標
    std::vector<FeatureId> feature;    // → FeatureDef 下標
    std::vector<uint8_t>   temperature;// 0-255
    std::vector<uint8_t>   moisture;   // 0-255
    std::vector<uint16_t>  elevation;  // 公尺
    std::vector<EdgeId>    edges;      // 每格四條邊各一個 EdgeDef 下標，打平：idx*4+dir
    std::vector<FactionId> owner;      // 勢力，0 = 無主
    std::vector<SiteState> site;       // 下層 Site 的具現化狀態（見下）
};
```

**沒有「Site 的 id」這種欄位。** Site 的身分就是它的 `ZoneKey`，
由 `(region_id, x, y)` **推導**而得——這正是座標定址方案要消滅的東西
（見 [zone-addressing.md](zone-addressing.md)）。這裡存的是狀態，不是身分：

```cpp
struct SiteState {
    LodLevel lod;            // 執行期，不序列化
    bool     ever_realized;  // 曾經具現化過 → 磁碟上有 digest
};
```

`SiteDigest` 與事件信箱不放在這條 SoA 裡——它們是稀疏的（多數 tile 沒有），
掛在旁邊的稀疏容器上。

理由：整欄運算（降水擴散、氣候迭代、影響力擴散）是全圖 stencil，SoA 對快取與 SIMD 友善；
序列化時每欄是一段連續記憶體，壓縮率也好。

**地形種類一律不是 enum。** `TerrainId`／`ReliefId`／`FeatureId`／`EdgeId` 都是強型別的
**資料檔定義下標**，種類數量無上限、擴展包可自由新增——完整規則見 [definitions.md](definitions.md)。

**河流與道路是「邊」不是「格」，而且共用同一套 `EdgeDef`。**
河從兩格之間流過，道路從邊上通過；灌溉渠、棧道、地下暗河都只是資料檔裡的另一筆 def。
每格存四條邊的 `EdgeId`（8 bytes／格，128×96 約 98 KB，可接受）——
比 4-bit 遮罩貴，換來的是種類無上限。相鄰兩格對同一條邊各存一份，
用 `set_edge(a, b, edge_id)` helper 保證不會只寫一邊。

## 三層地形，避免組合爆炸

顯示端用三個 TileMapLayer 疊出結果，而不是為每種組合出一張圖：

```
[地物層]  森林、礦脈、廢墟 icon，稀疏
[起伏層]  丘陵、山地紋路，半透明疊加
[基底層]  草原、沙漠、海洋……鋪滿
```

沙漠丘陵有森林 = 沙漠基底 + 丘陵起伏 + 疏林地物，三張小圖組出來。
設計來源與細節見 `~/repo/game_dev/my_godot_assists/godot_world_map/CONCEPT.md`。

## 移動

- 每個單位每旬有**移動點 MP**（整數，避免浮點漂移）。
- 進入一格的成本由各 def 的 `move_cost` 相加，再乘季節修正——**成本全部來自資料檔，不寫死**。
  範例：草原平原 = 2；丘陵 +2；山地 +4；密林 +3；冬季雪封山地 +6。
- **邊上的東西優先**：若移動所經的那條邊有 `EdgeDef`（道路、棧道、橋），
  成本改用該 def 的跨越成本。小數用「MP 以 2 為單位」的整數表示法迴避。
- **河流阻擋**：跨越河類 `EdgeDef` 需額外成本，除非該邊另有橋類 def。
- 海格只有船能進；港口是「陸格 + 港口建設」，是海陸轉運點。
- **命令是持續的**：下一次移動命令後，單位每旬自動推進，中途遇敵或遇事件才中斷。
  這是文明系列的節奏，不是每旬手動點一步。

## 尋路

| 情境 | 演算法 |
|---|---|
| Region 內 A → B | 四鄰接 A*，啟發函數用曼哈頓距離 × 最小格成本（admissible） |
| 跨 Region | 先在 WorldGraph 上跑 Dijkstra 得到 Region 序列，再逐段在各 Region 內 A* 走到出境點 |
| 大量單位同框 | 對常用起訖對做路徑快取；勢力 AI 用低精度的區塊圖（把 8×8 格聚成一個節點）先粗算 |

分層尋路的關鍵是**出境點（portal tile）**：Region 邊界上被標記的格子，
每個綁定一條 WorldGraph 的邊。跨 Region 的路徑天然被切成「到出境點 → 過邊 → 從入境點出發」三段。

## 世界圖：多個大地圖之間

```
節點 = 一個 Region（大陸／小世界）
邊   = 通道：海路、山口、地下通道、傳送門
```

邊的屬性：

| 欄位 | 說明 |
|---|---|
| `cost_ticks` | 通過需要幾旬 |
| `requirement` | 通行條件：需船隻等級 N、需鑰匙物品、需季節（冬季山口封閉）、需劇情旗標 |
| `portal_a` / `portal_b` | 兩端各自的出境點 tile 座標 |
| `capacity` | 同時可通過的部隊上限（可選，用於做戰略瓶頸） |

世界圖是**稀疏且手工可編**的——大陸不多（初期 3～5 個），
所以它是設計資料而非程序生成，用一份 JSON/TOML 描述，載入時建圖。

## 回合流程（一旬）

一旬結算的階段順序固定，**任何非確定性都不允許進入這條流水線**：

1. **玩家指令階段** — 收集本旬的所有命令（時鐘不動）
2. **命令執行** — 移動、建造下單、外交提案、部隊編成
3. **遭遇判定** — 移動路徑交錯者觸發遭遇，可能中斷移動或升級成戰鬥
4. **勢力 AI** — 各 NPC 勢力依同一套命令介面下指令並執行
5. **世界模擬** — 人口、糧食、資源、建設進度、影響力擴散、季節推移
6. **事件階段** — 依權重表抽事件（天災、商隊、劇情觸發）
7. **回合末** — 勝敗判定、自動存檔、時鐘 += 864,000 秒（1 旬）

第 5 步是 Region 級的**低解析度權威模擬**：不管有沒有 Site 被載入，它一直在跑。
這是三層一致性的基石，理由見 [interface-world-mid.md](interface-world-mid.md)。

## 勢力、城市、資源（大綱）

- **勢力（Faction）**：擁有 tile、部隊、資源池、外交關係、科技進度。玩家是其中之一。
- **城市（City）**：某個 tile 上的建設，持有人口、建設等級、糧食與生產產出、駐軍。
  城市在 Region 層是**一組數字**；放大成 Site 才有街廓與建築物件。
- **資源**：糧食、木材、石材、金屬、金錢、魔素。Region 層只記總量與流量，不記單筆交易。
- **視野**：每個勢力一張 fog bitmap（已探索／目前可見／未知），單位與城市提供視野半徑。

## 生成流程

```
板塊 → 高度場 → 侵蝕 → 氣候 → 河流 → biome → 地物
     → 歷史層 → 選址 → 道路 → 出境點 → 勢力起始
```

前七步（自然環境）見 [worldgen-terrain.md](worldgen-terrain.md)，
後五步（人文）見 [worldgen-civ.md](worldgen-civ.md)——
**歷史層排在選址之前**是刻意的，理由見該檔的「裁定」一節，
共通的階段契約與決定論要求見 [gen-pipeline.md](gen-pipeline.md)。

每階段用衍生子種子，改一階段不會洗掉整張圖（見 [tech-stack.md](tech-stack.md) 決定論一節）。

## 待細化

- 成本表、產出表、科技樹的完整數值
- 勢力 AI 的決策模型
- 外交與戰爭的規則
- 視野與 fog 的 per-faction 儲存方式
