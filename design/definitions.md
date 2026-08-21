# 定義系統（Def / Ruleset）

> **鐵律：地形、河流、道路、建築、單位這類「種類」一律不寫死成 C++ enum。**
> 它們是資料檔裡的定義物件，執行期以整數下標存取。
> 上層見 [tech-stack.md](tech-stack.md)；擴展層見 [rules-extensibility.md](rules-extensibility.md)。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 為什麼不用 enum

寫成 `enum class Terrain { Grass, Desert, ... }` 的代價，在種類變多時會全面爆開：

- 加一種地形要改 C++、重編、重跑所有 switch
- switch 漏一個 case 是安靜的 bug
- 擴展包無法新增種類
- 地形有屬性（移動成本、產出、通行規則），enum 承載不了，只好在旁邊再開一張平行表——兩份真相
- 河流、道路只會有更多種（灌溉渠、下水道、石板路、驛道、棧道、地下暗河……）

**medps 已經走過這一輪並拍板**（`medps/workflows/roadmap/fantasy-civ6.md` 的 D-2）：
def 全部從 ECS registry 撤出、進入不可變的 `Ruleset`、不進存檔、載入期建 id→下標索引。
aetheria 直接繼承這個結論。

## 形狀

```cpp
// 每一類 def 一個 POD struct，欄位具名、可手寫可 diff
struct TerrainDef {
    std::string  id;          // "terrain.grassland"，資料檔裡的權威識別
    std::string  name_key;    // 顯示名的 i18n key
    int32_t      move_cost;   // >= 1
    Yield        yield;
    uint32_t     flags;       // 可通行、阻擋視線、需船……
    VisualRef    visual;      // 給 Godot 的圖集索引，core 不解讀
};

class Ruleset {
public:
    const TerrainDef* terrain(TerrainId) const noexcept;   // O(1)，查不到回 nullptr
    TerrainId         find_terrain(std::string_view id) const noexcept;
    std::span<const TerrainDef> terrains() const noexcept; // 可迭代
private:
    std::vector<TerrainDef> terrains_;                     // 下標即 TerrainId
    // 載入期建立的 id 字串 → 下標索引
};
```

- **`Ruleset` 載入後不可變。** 只有載入器是 friend，其餘一律 `const&`。
  不可變讓它能安全地被所有 zone 共享、被多執行緒讀。
- **下標即 id。** `TerrainId` 是 `enum class TerrainId : uint16_t {}`（強型別，防混用），
  數值就是 `terrains_` 的下標，存取是一次陣列定址。
- **載入期一律 fail-fast。** 檔案打不開、格式壞、缺區段、id 重複、`move_cost < 1`——全部 throw，
  絕不回半套規則庫。半套規則庫造成的 bug 會在幾百格之外才炸開。

## 有哪些 def 型別

| def 型別 | 取代掉的 enum |
|---|---|
| `TerrainDef` | 基底地形（草原／沙漠／凍原…） |
| `ReliefDef` | 地形起伏（平原／丘陵／山地…） |
| `FeatureDef` | 地物（森林／礦脈／廢墟…） |
| `EdgeDef` | **河流與道路**——兩者都是邊上的東西，共用一套 def |
| `BuildingDef` | 建築 |
| `UnitDef` | 單位 |
| `ZoneKindDef` | Site 種類（城市／要塞／荒野…，見 [midmap.md](midmap.md)） |
| `EventDef` / `SkillDef` / `ItemDef` / `FactionDef` | 依系統陸續加 |

**河流與道路合併成 `EdgeDef` 是刻意的**：它們在資料結構上是同一件事（掛在兩格之間的邊、
有等級、影響跨越成本），差別只是屬性值。這讓「灌溉渠」「棧道」「地下暗河」都只是資料。

### ⚠ 缺一個 `OverlayDef`，而它有明確的到期日

M5.0 落地 L3 的 `overlay` 欄（地毯、血跡、雜物堆）時，Ruleset 裡沒有對應的 def，
實作端先加了一個**非持久**的 `OverlayId` 列舉。**現在這樣是可以的**——
非持久表示每次重算，不進位元流，也就沒有下標位移的風險。

但 [lowmap.md](lowmap.md) 的持久層表已經寫著**血跡「短期存，超時清理」**。
一旦 overlay 開始進存檔，那個列舉就直接違反下一節的「存字串 id 不存下標」。

> **裁定：`overlay` 成為持久之前，必須先改成走 `OverlayDef`。**
> 這是**順序約束**，不是「有空再做」——反過來做就是一次存檔破壞。

寫成順序而不是排程，是因為它不需要任何人記得日期：**要存 overlay 的那一輪自然會撞到它。**

## Tile 存的是什麼

[worldmap.md](worldmap.md) 的 SoA 欄位存的是 **def 下標，不是 enum 值**：

```cpp
std::vector<TerrainId> base;      // uint16_t
std::vector<ReliefId>  relief;
std::vector<FeatureId> feature;
```

邊資料因為一條邊要記「哪一種」而不只是「有沒有」，不能再用 4-bit 遮罩：

```cpp
// 每格四條邊各一個 EdgeId，0 = 無
std::array<std::vector<EdgeId>, 4> edges;   // 或打平成 vector，idx*4+dir
```

代價是每格 8 bytes（4 邊 × uint16）而不是 1 byte。128×96 的 Region 是 98 KB，可接受。
Site 層 64×64 是 32 KB，也可接受。**這筆記憶體換來的是河流道路種類無上限。**

## 存檔存字串 id

**存檔絕不存下標。** 下標會因為資料檔改動而整體位移，存了就等於把存檔綁死在某一版規則檔。

作法：存檔開頭寫一份 **id 表**（`下標 → 字串 id`），主體用下標；
讀檔時用當前 `Ruleset` 把舊下標重映射成新下標。

懸空 id（存檔有、規則檔沒有）的政策：**fail-fast，讀檔就報錯**，
明確告訴使用者存檔與規則檔不搭。開發期本來就是「格式一變就刪存檔」，
等到存檔成為玩家資產時再談遷移表。

## 資料檔佈局與 def 之間的引用

**獨立一檔** → [definitions-layout.md](definitions-layout.md)。
TOML 的目錄結構、載入順序，以及 def 互相引用怎麼解析都在那裡。

## 生成器與 Ruleset

程序生成需要知道「草原的 id 是多少」。兩種做法：

- ❌ 生成器寫死數值常數，靠測試防漂移（medps 現況，D-2a 點名這個面會放大）
- ✅ **生成器吃 `const Ruleset&`，用字串 id 查一次下標並快取**

aetheria 選後者。生成器啟動時做一次 `find_terrain("terrain.grassland")`，
拿到下標存進區域變數，之後迴圈裡用下標。一次字串查詢的成本可以忽略，
換來的是單一真相——加一種地形只要改資料檔。

## 與 EnTT 的分工

| 東西 | 住哪 | 為什麼 |
|---|---|---|
| **def（種類）** | `Ruleset`，不進 registry、不進存檔 | 幾千筆、不變、要 O(1) 查、改平衡要能回溯套用到舊存檔 |
| **instance（實例）** | `entt::registry` 的 entity + components | 會變、會生會滅、要進存檔 |

實例只帶一個 `BuildingId`（下標）指向它的 def。
**這條線畫錯的代價很大**：def 進 registry 就等於每份存檔夾著幾千個 def 實體，
改平衡無法套用到舊存檔——medps 的 D-2 就是在修這個。

## 待細化

- 各 def 型別的完整欄位
- `Ruleset` 的持有者（誰 own、生存期）
- 資料檔的 schema 驗證與錯誤訊息格式
- 擴展包的載入順序與覆寫語意
