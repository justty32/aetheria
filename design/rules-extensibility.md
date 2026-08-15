# 規則層的可擴展架構

> 前提：**遊戲規則之後會有大量、大型的擴展**。這份文件決定「加東西時要改什麼」，
> 目標是讓 90% 的擴展不必重編 C++。上層見 [tech-stack.md](tech-stack.md)。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 三級擴展模型

| 級別 | 擴展什麼 | 改哪裡 | 要重編嗎 | 佔比目標 |
|---|---|---|---|---|
| **L-Data** | 新建築、新地形、新單位、新資源、新事件、數值調整 | `data/*.toml` | 否，可熱重載 | ~70% |
| **L-Script** | 新技能效果、新事件邏輯、新 AI 行為、新勝利條件、劇情分支 | `scripts/*.lua` | 否，可熱重載 | ~25% |
| **L-Core** | 新的模擬子系統、新的地圖層、新的資料欄位 | `core/` C++ | 是 | ~5% |

設計時的自我檢查：**每提出一個新玩法，先問它能不能落在 L-Data 或 L-Script。**
如果答案是「不行，得改 core」，先想想是不是 core 的抽象缺了一塊。

## L-Data：資料驅動

所有靜態表格外置成 TOML，啟動時載入、驗證、建成 `constexpr` 友善的查表結構。

```
data/
  terrain.toml      地形、移動成本、產出修正
  buildings.toml    建築：尺寸、工時、成本、產出、相鄰加成、需求
  units.toml        單位：屬性、移動、戰力、招募條件
  factions.toml     勢力：起始條件、性格參數
  events.toml       事件：權重、條件（可指向腳本）、效果
  calendar.toml     季節對各項係數的修正
```

規則：

- **一律用字串 id，不用數字 id。** `"building.smithy"`，載入時才映射成 `uint16_t`。
  數字 id 一改就會讓舊存檔錯位；字串 id 讓擴展包可以自由插入。
- **存檔存字串 id**（或存一份 id 表），不存執行期的數字索引。
- 每張表有 **schema 驗證**，載入失敗要指出是哪個檔案哪一行哪個欄位，不能只說「載入失敗」。
- **擴展包可疊加**：`data/` 之後掃 `mods/<name>/data/`，同 id 覆寫、新 id 新增，順序由清單決定。

## L-Script：嵌入腳本

### 選型：Lua 5.4 + sol2

| 候選 | 取捨 |
|---|---|
| **Lua + sol2**（採用） | 綁定體驗最好、header-only、vcpkg 現成；使用者已熟悉 ToME4 的 Lua modding 生態 |
| AngelScript | 靜態型別、語法近 C++，但綁定樣板多、生態小 |
| Wren / ChaiScript | 輕巧但生態與工具鏈弱 |

sol2 讓「把 C++ 型別暴露給腳本」變成幾行宣告，配合現代 C++ 的 concepts 與
`std::function`，綁定層不會長成一大坨手寫膠水。

### 腳本掛在哪些點

```
事件：  condition(ctx) -> bool          effect(ctx) -> void
技能：  can_use(ctx) -> bool            apply(ctx, targets) -> void
建築：  on_complete(ctx)                on_tick(ctx)         modifier(ctx) -> table
AI：    evaluate(ctx, options) -> score
劇情：  on_trigger(ctx)
生成：  post_generate(region|site, rng)  ← 生成後的修飾，不取代主生成器
勝敗：  check(ctx) -> Outcome
```

`ctx` 是一個**受限視圖**：腳本只能讀被明確暴露的查詢、只能透過一組明確的動作寫入。
腳本拿不到 `RegionTiles` 的裸指標，也不能直接改 `owner`——只能呼叫 `ctx:set_owner(...)`，
由 C++ 端驗證合法性並記錄成事件。

### 決定論鐵律（最容易被腳本破壞的東西）

core 的決定論保證（見 [tech-stack.md](tech-stack.md)）必須延伸到腳本，否則整套回歸測試失效：

1. **移除非決定來源。** 沙箱化的 `_ENV` 裡沒有 `os.time`、`os.clock`、`os.date`、`io`、
   `math.random`、`math.randomseed`、`collectgarbage`。
2. **只用注入的 RNG。** `ctx.rng:int(a, b)`，背後是 core 的 `mt19937_64`，
   種子從當前 seed 衍生。腳本自己 `require` 不到任何亂數源。
3. **table 迭代順序。** Lua 的 `pairs` 順序不保證。提供 `ordered_pairs`（依 key 排序），
   並在 lint 階段禁止在會影響狀態的路徑上用 `pairs`。
4. **腳本不持有跨回合狀態。** 需要記住的東西一律寫進 core 的持久層。
   **存檔不序列化 Lua 狀態**——這一條同時解決了存檔相容性與決定論兩個問題。
5. **執行點固定。** 腳本只在回合流水線的固定階段被呼叫，沒有 coroutine 跨回合、沒有非同步。
6. **錯誤不吞。** 腳本拋錯要中止該次結算並回報，不能靜靜跳過——
   靜默失敗會讓兩次執行結果不同。

### 效能

腳本只跑在**低頻**路徑：回合階段、事件、技能結算。
高頻路徑（尋路、地形生成、逐格模擬、全圖 stencil）**一律留在 C++**。
若某個腳本掛勾被發現每回合呼叫上萬次，那是設計錯了，該把它上移成資料表或下沉成 C++。

## 熱重載

開發期價值極高：改一個數值或一段腳本，不必重啟遊戲。

- `data/` 與 `scripts/` 重載後，**重建規則表與 Lua state**，但**不動世界狀態**。
- 重載後跑一次驗證：所有存在的物件的 id 是否仍存在於新表中；缺 id 就報錯並拒絕重載。
- 正式版可關閉。

## 對 core 的要求

要讓上面這些成立，core 必須先做對兩件事：

1. **規則與狀態分離。** `core/rules/` 只有唯讀的表與純函數；世界狀態在 `core/world/`。
   規則物件不得持有可變狀態。
2. **狀態的欄位可擴充。** 建築、單位、勢力要能掛「擴展包自訂的欄位」。
   作法是每個實體帶一個 `std::vector<std::pair<PropId, int64_t>>` 的稀疏屬性袋，
   id 由資料表註冊。整數化以保決定論，需要小數就用定點。

## 待細化

- `ctx` 的完整 API 面（查詢與動作的清單）
- 擴展包的清單格式、相依與載入順序
- 腳本的 lint 規則與 CI 檢查
- 屬性袋的序列化與版本遷移
