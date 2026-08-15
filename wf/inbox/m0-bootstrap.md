# 信：任務書 M0 — 可編譯的骨架

**寄件人**：Opus 5 規劃者
**收件人**：**實作 agent**
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**前置**：先看 [`implementer-onboarding.md`](implementer-onboarding.md)

---

## 這份任務要證明什麼

**只證明工具鏈打得通，不實作任何玩法。**

M0 的價值全在於「早點撞到編譯器」。設計文件寫了三十幾份，
但沒有一行程式碼編譯過——`core` 與 `bridge` 的 C++23／ABI 一致性、
vcpkg 與 godot-cpp 的共存、headless 測試，這四件事都可能有我沒預見的坑。

**做出來能跑的最小東西就好。醜沒關係，能編、能跑、能測就行。**

## 目錄佈局

```
aetheria/
  core/          純 C++23，零 Godot 依賴
  bridge/        godot-cpp 綁定層
  godot/         Godot 4 專案
  tests/         GoogleTest
  sim/           headless CLI
  vcpkg.json     manifest 模式，鎖 baseline
  CMakeLists.txt 頂層
  third_party/godot-cpp/   submodule 或固定版本原始碼樹
```

`design/`、`wf/` 已存在，不要動。

## 建置目標

| target | 種類 | 連什麼 | 硬性要求 |
|---|---|---|---|
| `aetheria_core` | 靜態庫 | 只有 vcpkg 依賴 | **include 路徑不得出現任何 godot-cpp** |
| `aetheria_tests` | 執行檔 | core + gtest | 能跑、能綠 |
| `aetheria_sim` | 執行檔 | core + cli11 | headless，不需要 Godot |
| `aetheria_bridge` | 共享庫 | core + godot-cpp | 產出 `.so` 給 Godot 載入 |

**`aetheria_core` 那條是最重要的驗收點。** 用 CMake 的 target 可見性擋死，
不要只靠約定——這是 [`design/tech-stack.md`](../../design/tech-stack.md) 鐵律一的機械化落實。

## 最小內容（真的很少）

`core/` 裡只要有能證明管線通了的東西：

```cpp
// core/time/tick.h
enum class Tick : int64_t {};          // 單位：秒，見 design/outline.md

namespace aetheria::time {
    constexpr Tick kSecondsPerXun = Tick{864'000};   // 1 旬 = 10 天
    struct CalendarDate { int32_t year; uint8_t season, month, xun; };
    CalendarDate to_date(Tick) noexcept;             // 360 天曆
}
```

```cpp
// core/api/version.h
namespace aetheria { const char* core_version() noexcept; }
```

`tests/` 至少兩個測試：曆法換算正確、往返一致。
`sim/` 印出版本與幾個曆法換算結果就夠。
`bridge/` 暴露一個 `AetheriaCore` Node class，有 `get_core_version()` 與
`tick_to_date(int64) -> Dictionary` 兩個方法。
`godot/` 一個場景，`_ready()` 裡呼叫這兩個方法並 `print`。

**就這樣。不要多做。**

## vcpkg

manifest 模式，`vcpkg.json` 進版控並鎖 baseline commit。M0 只需要：

```
gtest, cli11
```

其餘（entt、tomlplusplus、lua、sol2、zstd、spdlog、benchmark）**等真的用到再加**。
清單見 [`design/cpp-conventions.md`](../../design/cpp-conventions.md)。

**godot-cpp 不走 vcpkg**——它必須跟 Godot 版本綁死，用 submodule 或固定版本原始碼樹。

## 已知風險與退路

### 風險一：godot-cpp 的 C++23 重編（最主要）

godot-cpp 預設 C++17。要跟 core 一致就得用相同標準與相同 ABI 旗標重編一份。

**使用者確認這條可行**——他先前用 C++20 重編過 godot-cpp。照做即可。
要守的是「core、bridge、godot-cpp 三者的標準與 ABI 旗標完全一致」，
混用會讓 `std::string`／例外／`std::expected` 的佈局在跨 TU 時對不上。

**退路**（真的走不通才用）：把 `bridge/` 的介面壓成 C ABI 風格的 POD，
讓 core 的現代特性不外洩到邊界。**用了退路一定要寫信告訴我**，
它會影響後續所有 bridge 介面的形狀。

### 風險二：C++26

`design/cpp-conventions.md` 說「C++23 基線，工具鏈支援時可用 C++26」。
**M0 一律用 C++23**，別碰 26——先讓基線跑起來。

### 風險三：Godot 版本

我沒有指定 Godot 版本。**請你確認機器上實際裝的是哪一版**，
用它，並在回信裡告訴我版本號——我會寫進設計文件。
`~/repo/game_dev/my-rpg-frontend` 與 `~/repo/game_dev/medps` 都有可運作的
GDExtension 設定，先去看它們用的是什麼版本。

## Done when

逐條核對，回信時逐條回覆：

- [ ] `cmake --build` 一次把四個 target 全部編出來，**零警告**（開 `-Wall -Wextra`）
- [ ] `aetheria_tests` 跑起來全綠，且**至少有一個測試會因為改壞曆法而失敗**（確認測試真的在測）
- [ ] `aetheria_sim` 能在**沒有 Godot** 的環境下跑起來並印出正確的曆法換算
- [ ] Godot 編輯器能載入 `godot/`，執行場景後 console 印出 core 版本與曆法換算結果
- [ ] `aetheria_core` 的 CMake target 上**確實沒有** godot-cpp 的 include 路徑
      （請貼出你怎麼驗證的——例如 `cmake --graphviz` 或編譯命令的 `-I` 清單）
- [ ] `vcpkg.json` 鎖了 baseline，**乾淨機器上 clone 後能重現建置**（至少論證這點）
- [ ] 寫一份 `design/build.md`（≤ 8 KB，繁體中文）記錄實際的建置步驟與踩到的坑

最後一項別省。下一個接手的人（包括我）需要它。

## 不要做的事

M0 的範圍控制很重要，**以下一律不做**：

| 不做 | 為什麼 |
|---|---|
| `Zone`／`ZoneManager` | 留給 M0.5，設計在 [`design/zone-model.md`](../../design/zone-model.md) |
| 任何生成器 | 留給 M1 |
| EnTT、cereal、Lua、TOML 的接入 | 等有東西要用它們的時候 |
| 任何玩法邏輯 | M0 不碰玩法 |
| 美術資源 | 尚未規劃 |
| `git init` 或 commit | **這個 repo 目前刻意還沒版控**，要開始版控請先問使用者 |

看到「順手把 X 也做一下」的念頭時，忍住，寫進回信的建議欄。

## 回信給我

做完照 [`implementer-onboarding.md`](implementer-onboarding.md) 的「回報方式」寫信。
我最想知道的三件事，請務必寫：

1. **godot-cpp 的 C++23 重編實際怎麼做的**（指令、改了什麼、花多久）
2. **設計文件裡有哪幾條是錯的或行不通的**（附證據）
3. **你做了哪些我沒交代的決定**（Godot 版本、目錄命名、CMake 結構…）

第 2 點最有價值。我的設計沒撞過編譯器，**你撞到的現實比我的推演可靠**。
