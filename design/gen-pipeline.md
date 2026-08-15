# 程序生成：共通契約

> 三層地圖的生成器共用同一套骨架與同一套規矩。這份講規矩，
> 各層的實際演算法在 [worldgen-terrain.md](worldgen-terrain.md)、[worldgen-civ.md](worldgen-civ.md)、
> [sitegen-city.md](sitegen-city.md)、[sitegen-wild.md](sitegen-wild.md)。
> 相鄰地圖的邊界如何對齊，見 [edge-consistency.md](edge-consistency.md)。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 階段即純函數

生成管線是一串**純函數**，不是一個大迴圈：

```cpp
struct StageInput  { /* 前一階段的產物 + Region 慢變數 */ };
struct StageOutput { /* 這一階段新增的欄位 */ };

StageOutput stage_climate(const StageInput&, uint64_t stage_seed, const Ruleset&);
```

三條硬性要求：

1. **無副作用**——不寫全域狀態、不讀時鐘、不碰檔案系統。
2. **可獨立測試**——每個階段各自有單元測試，輸入固定則輸出逐位元固定。
3. **可獨立視覺化**——每個階段都能把中間產物 dump 成一張灰階圖或色圖。
   除錯地形生成器沒有這個等於瞎子摸象。

## 種子衍生

```cpp
uint64_t stage_seed  = splitmix64(world_seed ^ STAGE_ID);
uint64_t region_seed = splitmix64(world_seed ^ REGION_SALT ^ region_id);
uint64_t site_seed   = splitmix64(region_seed ^ (uint64_t(y) << 16 | x));
```

**每個階段拿自己的子種子，不共用一條 RNG 串流。**

理由：若整條管線共用一個 `mt19937_64`，那麼改動階段 3 的抽樣次數，
會讓階段 4 之後**全部**跟著位移——調一個森林密度參數，整個大陸的城市位置就變了。
子種子隔離讓每個階段可以獨立調參，這在內容製作期是天差地別的體驗。

同理，**階段內部若有多個子系統，各自再衍生一次**（河流的起點抽樣與流量計算各一條）。

## 降維裁決鏈

這是整套生成保持一致的核心規則，也是 [edge-consistency.md](edge-consistency.md) 的地基：

> **共用的東西由更低維的物件裁決。每一層只能讀比自己低維的裁決結果，不能反向寫。**

```
點（0 維）  角點高度、穿越點位置        ← 最先決定，所有人都得服從
  ↓
邊（1 維）  邊界剖面、河道與道路的穿越   ← 只能讀點
  ↓
面（2 維）  地圖內部                     ← 只能讀點與邊
```

推論：Site 生成器**永遠不寫**自己的邊界——邊界是它的**輸入**（邊界條件），
由邊的裁決給定。這讓兩個相鄰 Site 可以獨立生成、以任意順序生成、
甚至其中一個從未被生成過，邊界仍然一致。

## 骨架與填充分離

呼應 [interface-world-mid.md](interface-world-mid.md) 的骨架穩定性鐵律：

| 階段 | 吃什麼 | 輸出 | 何時重算 |
|---|---|---|---|
| `build_skeleton` | **只有慢變數** + seed + 邊界條件 | 地形、水系、主幹道、可建地 | 慢變數變動時（罕見） |
| `populate` | 骨架 + **快變數** | 填充物、建築密度、無名 NPC、物件狀態 | 每次重載 |

用**函式簽章**擋死：`build_skeleton` 拿不到快變數。
這條保證了持久層物件的座標永遠有效——你在 (17,32) 蓋的工坊，
不會因為人口暴跌而發現那格變成了河。

## 生成器吃 Ruleset

生成器需要知道「草原的 id 是多少」。**不寫死數值常數**：

```cpp
// 生成器啟動時查一次，之後迴圈裡用下標
const TerrainId kGrass = rules.find_terrain("terrain.grassland");
```

一次字串查詢的成本可忽略，換來的是單一真相——加一種地形只要改資料檔。
medps 的 D-2a 正是在處理「worldgen 寫死常數、靠測試防漂移」帶來的維護面放大，
aetheria 一開始就不走那條路。詳見 [definitions.md](definitions.md)。

查不到 id → **fail-fast**。生成器啟動時就把它需要的 def 全查一遍，
缺一個就 throw 並指出是哪個 id——不要生成到一半才發現。

## 效能預算

| 生成對象 | 目標時間 | 何時發生 |
|---|---|---|
| 一個 Region（128×96） | < 3 秒 | 開新遊戲，可顯示進度條 |
| 一個 Site（64×64） | **< 30 毫秒** | 玩家進城的當下，不能卡 |
| 一個 Local（64×64） | **< 10 毫秒** | 串流中，每幾步就要生一個 |

Region 的預算寬鬆（一次性）；Site 與 Local 的預算很緊，因為它們是**互動中**發生的。

推論：Site 與 Local 的生成器**不能用迭代式演算法**（侵蝕模擬、鬆弛迭代、
wave function collapse 的回溯）。它們必須是單趟的、可預測成本的。
複雜的東西留給 Region 層。

超出預算時的退路是**背景執行緒預生成**：玩家靠近某格時就先在背景把 Site 生出來。
這要求生成器是純函數（沒有副作用才能安全地在別的執行緒跑）——
這也是「階段即純函數」那條規矩的第二個好處。

## 決定論的驗證

生成器是決定論要求最嚴的地方，因為它的輸出量最大、最難靠肉眼發現漂移：

| 測試 | 判準 |
|---|---|
| **同 seed 逐位元相同** | 同一個 seed 生成兩次，所有欄位的雜湊相同 |
| **階段隔離** | 改動階段 N 的參數，階段 1..N−1 的輸出雜湊不變 |
| **順序無關** | 以不同順序生成多個 Site，各自的結果不變 |
| **邊界一致** | 見 [edge-consistency.md](edge-consistency.md) 的四項測試 |
| **平台一致** | 同 seed 在 GCC 與 Clang、Linux 與 Windows 下結果相同 |

**平台一致這條要特別小心**：它是「遊戲狀態不用浮點數」那條約定的真正理由。
浮點數的求值順序、`-ffast-math`、超越函式的實作差異，都會讓跨平台的結果分岔。
生成階段的中間計算可以用浮點（噪聲、梯度），但**寫進世界狀態前必須量化成整數**，
且量化必須在明確的一點發生，不能散落各處。

## 除錯工具

`aetheria_sim`（headless CLI，見 [cpp-conventions.md](cpp-conventions.md)）應提供：

```bash
aetheria_sim gen region --seed 12345 --dump-stages out/    # 每階段一張 PNG
aetheria_sim gen site   --region 3 --tile 17,32 --dump out/
aetheria_sim gen verify --seed 12345 --iterations 100      # 決定論回歸
```

不開 Godot 就能檢視生成結果，是「core 不依賴引擎」買到的最實際的好處之一。

## 待細化

- 各階段的 `STAGE_ID` 常數表
- 中間產物的 dump 格式
- 背景預生成的觸發距離與執行緒模型
- 量化點的具體位置（哪一步從浮點轉整數）
