# 接邊一致性

> 相鄰的兩個 Site 各自獨立生成，它們的共用邊界必須嚴絲合縫：
> 河從 A 的東邊流出，就得從 B 的西邊流進來，位置、寬度、流向全都要對得上。
> 這是 [gen-pipeline.md](gen-pipeline.md)「降維裁決鏈」的具體落地。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 問題

Region 的兩個相鄰 tile 各自展開成 Site。要對齊的東西有四類：

| 類別 | 不對齊時的症狀 |
|---|---|
| 高度 | 邊界出現一道懸崖 |
| 地面／水體 | 海岸線在邊界斷掉，或水面高度不一致 |
| 河流與道路 | 路走到邊界就消失，河流憑空斷流 |
| 角落 | 四個 Site 共用的那一點有四個不同的高度 |

而且困難的地方在於：**B 可能永遠不會被生成**（玩家從來沒去過），
或者 B 比 A 早生成，或者 A 被卸載後重生成而 B 一直活著。
任何「A 生成時去問 B」的方案都會壞掉。

## 解法：邊界由更低維的物件裁決

> **Site 永遠不寫自己的邊界。邊界是它的輸入。**

```
角（0 維）  四個 Site 共用 → 由角自己裁決
  ↓  只能讀
邊（1 維）  兩個 Site 共用 → 由邊自己裁決，兩端錨定到角
  ↓  只能讀
面（2 維）  Site 內部 → 自由發揮，但必須滿足邊界條件
```

兩個 Site 不互相溝通，它們共同服從一個**第三方**。
所以生成順序無關、缺席無關、重生成無關——這三件事是同一個保證的三種說法。

## 規範化識別碼

一致性的全部重量壓在這裡：**兩側必須算出同一個 id**，否則就會拿到兩個不同的 seed。

```cpp
// tile 線性索引 idx = y * w + x
// 從較小的 idx 出發，鄰居必定在東或南（row-major 的性質）
constexpr uint64_t canonical_edge_id(uint32_t idx_a, uint32_t idx_b, uint32_t w) {
    uint32_t lo = std::min(idx_a, idx_b);
    bool     is_south = (std::max(idx_a, idx_b) - lo) == w;   // 否則差 1，是東
    return uint64_t(lo) * 2 + is_south;
}

// 角點座標範圍是 [0..w] × [0..h]（比 tile 多一排）
constexpr uint64_t canonical_corner_id(uint32_t cx, uint32_t cy, uint32_t w) {
    return uint64_t(cy) * (w + 1) + cx;
}
```

```cpp
uint64_t edge_seed   = splitmix64(region_seed ^ EDGE_SALT   ^ canonical_edge_id(...));
uint64_t corner_seed = splitmix64(region_seed ^ CORNER_SALT ^ canonical_corner_id(...));
```

`min`/`max` 讓兩側必然得到同一個值。這是整套機制唯一的技巧，其餘都是它的推論。

## 方向的陷阱

同一條邊，A 從自己的角度看是「東邊界」，B 看是「西邊界」。
若兩者以各自的本地順序取樣，會得到**互為反序**的剖面——邊界看似對齊，實則左右顛倒。

規約：**剖面永遠以規範方向索引，不以本地方向。**

| 邊的走向 | 規範索引方向 |
|---|---|
| 垂直邊界（東西相鄰） | index 隨 **y 遞增**（北 → 南） |
| 水平邊界（南北相鄰） | index 隨 **x 遞增**（西 → 東） |

各 Site 在讀取時自行把本地邊座標映射到規範索引，必要時反轉。
**這一步是最容易寫錯又最難用肉眼發現的地方**，要有專門的單元測試。

## 邊界剖面

```cpp
struct BoundaryProfile {
    std::array<uint16_t, 64> elevation;   // 公分或公尺，量化後的整數
    std::array<GroundId, 64> ground;
    std::array<uint8_t,  64> water_depth; // 0 = 無水
    std::array<EdgeId,   64> edges;       // 邊上的東西：牆、柵欄、城牆、懸崖；0 = 無
    std::vector<Crossing>    crossings;   // 通常 0～2 個
};

struct Crossing {
    uint8_t  pos;      // 0..63，沿規範方向的位置
    uint8_t  width;    // 佔幾格
    EdgeId   kind;     // 來自 Region 的 edges 陣列：哪一種河／路／橋
};
```

`edges` 那一欄容易被忽略但**不可省**：牆會壓在 zone 邊界上。
一段城牆若沿著 Site 邊界走、或一棟房子橫跨兩個 Local，
它的牆就落在共用邊上——兩側必須看到同一段牆。
牆與河流道路共用同一套 `EdgeDef`（見 [definitions.md](definitions.md)），
所以這一欄用的是同一個型別。

生成順序：

1. **兩端的角點**先算（`corner_seed` → 高度、地面）
2. **中間 62 格**由 `edge_seed` 驅動的一維噪聲產生，**兩端硬錨定到角點值**
3. **Crossing 從上層的邊資料來**——上層 tile 的 `edges[idx*4+dir]` 若不是 0，
   這條邊上就有一條河或路，其種類即該 `EdgeId`；穿越位置由 `edge_seed` 抽定
4. **`edges` 欄位**同樣由上層裁決（城牆／房屋隔牆由上層的 `structure` 與 `zoning` 決定），
   細部走向由 `edge_seed` 補完

第 3 點值得強調：**河流與道路的「有沒有」不是 Site 生成器決定的，是 Region 早就決定的。**
Region 層在 worldgen 時就把河道與道路寫進了 `edges`（見 [worldgen-terrain.md](worldgen-terrain.md)），
Site 只是把它放大成具體的河床與路面。這讓大地圖上看得到的河，
下鑽進去必定找得到，反之亦然。

## 地形過渡

若邊界兩側的 Region tile 地形不同（草原接沙漠），剖面的 `ground` 不該是硬切線：

- 沿剖面用 `edge_seed` 產生一條擾動閾值，逐格決定該點屬於哪一側的地形，
  形成犬牙交錯的自然過渡
- 兩側的 Site 各自從自己的內部地形，在靠近邊界的若干格內**漸變**到剖面給定的值

因為擾動來自共用的 `edge_seed`，兩側算出的過渡線完全相同。

## Region 邊緣的 Site

Region 最外圈的 tile，有些邊沒有鄰居。兩種情況：

| 情況 | 邊界剖面來自 |
|---|---|
| 地圖外緣（世界的盡頭） | 固定規則：海洋或不可通行的屏障（高山、虛空），由 Region 的設定決定 |
| **出境點（portal tile）** | [worldmap.md](worldmap.md) 的 WorldGraph 邊。相連的兩個 Region **在空間上不相鄰**，所以**不要求幾何連續**——只要求敘事連續（碼頭對碼頭、山口對山口） |

跨 Region 不做幾何對齊是刻意的。強求兩個獨立生成的大陸在像素層級接得上，
成本極高而玩家永遠看不到（中間隔著一段海路或山道）。

## 遞迴到 L3

L2 → L3 用**完全同一套機制**，只是尺度降一級：
Site 的 tile 之間有邊與角，Local 生成時同樣先讀角、再讀邊、最後填面。
`canonical_*_id` 的公式不變，只是 `w` 換成 64、`region_seed` 換成 `site_seed`。

這也意味著 L2 與 L3 的邊界一致性可以共用同一份實作與同一組測試。

## 驗證測試

這四項是接邊機制的完成判準，缺一不可：

| 測試 | 內容 | 判準 |
|---|---|---|
| **雙側一致** | 生成 A 與 B，取出共用邊的剖面 | `elevation` / `ground` / `water_depth` / **`edges`** / `crossings` 逐位元相同 |
| **順序無關** | 先 B 後 A，與先 A 後 B | 兩次的 A、B 內容各自逐位元相同 |
| **缺席無關** | 只生成 B（A 從未存在） | B 的西邊界 == A 存在時算出的東邊界 |
| **角點四方一致** | 生成共用某角的四個 Site | 該角的高度與地面在四者中相同 |

再加一項容易漏的：

| **方向不顛倒** | 造一條沿剖面單調遞增的高度，檢查兩側讀到的遞增方向 | 兩側的實際地形走向一致，不是鏡像 |

## 待細化

- `EDGE_SALT` / `CORNER_SALT` 常數
- 一維噪聲的實際函式與參數（要能在兩端硬錨定）
- 漸變帶的寬度（幾格內從內部地形過渡到邊界值）
- Crossing 進入 Site 之後如何延伸到內部（河道走向、道路接入主幹道）
- 高度的量化單位與精度
