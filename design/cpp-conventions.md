# C++ 標準、寫法約定與依賴

> 承接 [tech-stack.md](tech-stack.md) 的三層架構，這份講「core 用什麼寫、怎麼寫、靠什麼建置」。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 標準版本

| 目標 | 標準 |
|---|---|
| `core/` | **C++23 為基線**，工具鏈支援時可用 C++26 特性 |
| `bridge/` | 同 core，godot-cpp 一併以相同標準重編 |
| 編譯器 | GCC 14+ / Clang 18+ |

godot-cpp 預設以 C++17 建置，要跟 core 一致就得用相同標準與相同 ABI 旗標重編一份。
**這條路已知可行**——使用者先前就用 C++20 重編過 godot-cpp。照做即可，
唯一要守的是「core 與 bridge 與 godot-cpp 三者的標準與 ABI 旗標必須完全一致」，
混用會讓 `std::string`／例外／`std::expected` 的佈局在跨 TU 時對不上。

**不用 C++20 modules。** 誘惑很大，但 godot-cpp、vcpkg 生態與 CMake 的模組支援都還不夠穩，
為此付出的建置系統代價超過收益。標頭檔配合 PCH 與 unity build 已經夠快。

## 寫法約定

寫給熟 C++ 的人看，所以只列**本專案的具體約定**，不重複通識。

### 型別

- **強型別 id，禁止裸整數。** `ZoneKey`、`FactionId`、`EntityId`（zone 內，實質是 `entt::entity`）、
  `EntityRef`（跨 zone 的弱引用）。混用 `uint32_t` 遲早會把 `FactionId` 傳進吃 `EntityId` 的函式。
  **沒有 `SiteId`**——Site 的身分就是它的 `ZoneKey`，由座標推導。
- **「種類」一律不是 enum，是資料檔定義的下標。** `TerrainId`、`EdgeId`、`BuildingDefId`…
  同樣是 `enum class X : uint16_t {}`，但**枚舉子一個都不列**——數值來自 `Ruleset` 的載入結果。
  這是本專案最容易被違反的一條：看到有人寫 `enum class Terrain { Grass, Desert }` 就擋下來。
  完整規則見 [definitions.md](definitions.md)。
- **座標是值型別**：`RegionXY`／`SiteXY`／`LocalXY`（三層各一個，不共用），
  帶 `operator<=>`、`constexpr` 建構、`std::hash` 特化。完整型別表見 [glossary.md](glossary.md)。
- **時間分兩種**：`Tick`（時刻）與 `Duration`（時距），都是 `enum class X : int64_t {}` 的秒，
  但**語意不同、不可互換**——時刻相加沒有意義。stride 常數一律是 `Duration`。
  **不准出現裸 `int` 的「回合數」**——回合長度是可變的 stride（見 [outline.md](outline.md)），
  把回合數當時間會在交戰時全面錯亂。合法運算表、合法域與 fail-fast 契約見
  **[time-model.md](time-model.md)**。
- **不用浮點數表示遊戲狀態。** 人口、資源、進度、機率權重一律整數或定點
  （定點用 `int32_t` + 固定 scale 1000，並封成 `Fixed` 型別）。浮點只准出現在顯示層與生成階段的中間計算。

### 介面

- 傳參用 `std::span<const T>`、`std::string_view`；回傳集合用 `std::vector` 或 ranges view。
- 錯誤處理用 `std::expected<T, Error>`。**例外只用於真正的程式錯誤**（不變式被破壞），
  不用於「玩家下了非法命令」這種預期內的失敗。
- 用 concepts 表達介面約束，不用 SFINAE、不用純虛擬繼承做靜態多型。
  執行期多型（例如不同的 Site 生成器）才用虛擬函式。
- 所有權清楚：`unique_ptr` 表示擁有、裸指標／參考表示借用、**不用 `shared_ptr` 表示世界狀態**
  （共享所有權會讓存檔與決定論都變糊）。實體之間的關聯一律用 id，不用指標。

### 資料佈局

- 格子資料用 **SoA**（見 [worldmap.md](worldmap.md)）。
- 異質實體（Actor、Building、Item）用 **EnTT**（entity-component）。
  **注意決定論**：EnTT 的 view 迭代順序取決於 pool 的內部排列，
  凡是會影響世界狀態的走訪，都要先取出 entity 再依穩定 key 排序，或用固定順序的 group。
  這條要寫進 code review 檢查表。
- 需要有序容器時用 `std::map`／`std::vector` + 二分，**不用 `unordered_map` 做順序敏感的迭代**。

### 風格

- 命名沿用標準庫風格：`snake_case` 函式與變數、`PascalCase` 型別、`kPascalCase` 或全大寫常數擇一。
- `[[nodiscard]]` 用在所有回傳 `expected` 與查詢函式上。
- 不變式用 `assert` 表達，release 版保留關鍵的幾條（用自訂 `AETH_CHECK`）。
- 每個公開型別的標頭檔頂端寫三行：這是什麼、誰擁有它、什麼時候失效。

## 依賴與建置

**用 vcpkg（manifest 模式）**，`vcpkg.json` 進版控，鎖 baseline commit 保證可重現。

| 用途 | 套件 | 備註 |
|---|---|---|
| 腳本 | `lua`、`sol2` | 規則擴展層，見 [rules-extensibility.md](rules-extensibility.md) |
| 實體管理 | `entt` | header-only |
| 資料表 | `toml11` 或 `tomlplusplus` | 偏好 `tomlplusplus`（現代 C++、錯誤訊息好） |
| 存檔序列化 | `zstd` + 自寫二進位 | 不用通用序列化庫，格式要能自己控版本 |
| 測試 | `gtest` | core 的單元測試 |
| 基準測試 | `benchmark` | 尋路、生成、全圖 stencil |
| 日誌 | `spdlog` | 僅 core 內部與 headless 工具用；不經 Godot |
| 命令列工具 | `cli11` | headless 模擬器 |

**godot-cpp 不走 vcpkg**，用 submodule 或固定版本的原始碼樹，因為它必須跟 Godot 版本綁死。

### 建置目標

```
aetheria_core        靜態庫，純 C++，無 Godot 依賴
aetheria_tests       gtest，連 core
aetheria_sim         CLI headless 模擬器，連 core
aetheria_bridge      共享庫 .so/.dll，連 core + godot-cpp → 給 Godot 載入
```

`aetheria_core` 的 CMake target 上**不得出現任何 godot-cpp 的 include 路徑**——
這是「core 不依賴引擎」那條鐵律的機械化落實，靠建置系統擋，不靠自律。

## 腳本語言的定位

腳本（Lua）只出現在**規則擴展層**，不是核心邏輯的實作語言。
工具鏈周邊（資料表產生器、素材處理、測試輔助）可以隨意用 Python 或 shell，不受限制。
詳見 [rules-extensibility.md](rules-extensibility.md)。
