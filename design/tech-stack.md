# 技術棧與程式碼分層

> 大綱層文件。回答「哪些東西寫在哪一邊、用什麼語言、怎麼測」。
> 上層見 [outline.md](outline.md)。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 三層程式碼架構

```
godot/     Godot 4 專案：場景、素材、GDScript 顯示層
   ↑  只能透過 GDExtension 的節點類別溝通
bridge/    godot-cpp 綁定層：Variant ⇄ core 型別、註冊 Node class、批次打包
   ↑  只能呼叫 core 的公開介面
core/      純 C++（C++23）。零 Godot 依賴。玩法邏輯全部在這裡。
```

### 為什麼 core 不碰 godot-cpp

對資深 C++ 而言這是最有價值的一條決策：

- **能用一般的 C++ 工具鏈。** GoogleTest、sanitizer、fuzzer、perf、valgrind 全部可用，
  不必為了跑一個單元測試而起 Godot。
- **編譯快。** godot-cpp 的 header 很重；core 改一行不必連帶重編綁定層。
- **可 headless 跑完整場遊戲。** 「模擬 1000 旬看經濟會不會爆」是一支 CLI 執行檔的事。
- **引擎可替換。** 真要換引擎或做伺服器版，換掉 `bridge/` 即可。

代價：core 不能用 `Vector2`、`String`、`Ref<>`、`UtilityFunctions::print`。
core 用 `std::` 與自訂 POD 型別，轉換一律在 bridge 做。**這條沒有例外。**

## core/ 模組總覽

| 模組 | 職責 |
|---|---|
| `core/time/` | 全局時鐘（`Tick` = 秒）、曆法換算、各層可變 stride 的回合排程 |
| `core/zone/` | `Zone`／`ZoneManager`、`ZoneKey` 定址、載入卸載 → [zone-model.md](zone-model.md) 與 [zone-addressing.md](zone-addressing.md) |
| `core/world/` | 世界圖、Region／Site／Local 各層的資料模型與 system（掛在 zone 上跑） |
| `core/gen/` | 程序生成：世界、Region 地形、Site 佈局、Local 佈局 |
| `core/path/` | 尋路：tile A*、region graph 上的 Dijkstra、分層尋路 |
| `core/sim/` | 模擬：人口、經濟、勢力、季節 |
| `core/event/` | 事件模型、跨層縮放（升格／降格）、事件信箱 → [events.md](events.md) |
| `core/lod/` | 觀察點場強、LOD 升降、預算逐出 → [observer.md](observer.md) |
| `core/rules/` | 規則：屬性、檢定、戰鬥、裝備（跨層共用） |
| `core/script/` | Lua 沙箱與掛勾點，規則擴展層的執行引擎 |
| `core/data/` | 資料表載入、schema 驗證、擴展包疊加 |
| `core/save/` | 序列化與版本遷移 |
| `core/api/` | **對外唯一介面**：命令型別、事件型別、快照查詢 |

規則之後會有大量擴展，因此 `core/rules/`、`core/data/`、`core/script/` 三者的分工
是獨立的一份設計：[rules-extensibility.md](rules-extensibility.md)。
C++ 標準、寫法約定與 vcpkg 依賴清單見 [cpp-conventions.md](cpp-conventions.md)。

`bridge/` 只准 include `core/api/`。這條靠目錄約定加 code review 守，
必要時用 CMake 的 target 可見性擋死。

## 跨語言邊界契約

邊界呼叫成本高，所以形狀刻意做得很窄：

```
GDScript ──submit_command(Dictionary)──▶ bridge ──▶ core
GDScript ◀──poll_events() -> Array───── bridge ◀── core（事件佇列）
GDScript ──fetch_layer(...) -> PackedByteArray──▶ 批次拉整層地圖
```

三條規則：

1. **命令是意圖，不是結果。** GDScript 送「這個單位要移動到 (x,y)」，
   不送「把這個單位的座標設成 (x,y)」。合法性由 core 判。
2. **事件是已成事實。** core 吐出的每個事件都已經改過狀態，GDScript 只負責演出。
   演出可以慢慢播（動畫），狀態早就對了。
3. **地圖資料批次拉。** 絕不逐格 `get_tile(x, y)` 跨邊界——
   12288 格 × 每格一次呼叫是效能災難。一次拉一整層 `PackedByteArray`，GDScript 端自己 unpack。
4. **bridge 必須驗證所有跨界輸入。** GDScript 傳進來的值是**外部輸入**，
   不是內部不變式。core 的 `AETH_CHECK`（見 [time-model.md](time-model.md)）是給
   **程式錯誤**用的，它會 `abort`——實測從 GDScript 傳一個域外的 tick 進去，
   整個 Godot 進程以 134 結束，沒有例外、沒有 Godot 的錯誤攔截。
   所以「非法值到不了 core」是 **bridge 的責任**：擋不下來就回錯誤／空值，
   不是讓它穿過去炸掉引擎。這正是 [cpp-conventions.md](cpp-conventions.md)
   「例外只用於真正的程式錯誤，不用於玩家下了非法命令這種預期內的失敗」的邊界落點。

## Godot 端做什麼

- **TileMapLayer 堆疊**畫地圖（基底／起伏／地物三層，設計理由見 `~/repo/game_dev/my_godot_assists/godot_world_map/CONCEPT.md`）
- 相機、選取高亮、小地圖、UI、對話框、音效
- 把使用者輸入翻成命令，把事件翻成動畫

**Godot 端不准有的東西**：任何「只存在於場景裡」的玩法狀態。
自我檢查法——把場景整個 free 掉再從 core 重建，畫面必須完全一致。

## 可複用元件來源與外部函式庫

**獨立一檔** → [tech-reuse.md](tech-reuse.md)。
現成 Godot 元件、GDExtension 佈局，以及 **libtcod 評估過不採用**的理由都在那裡。

## 測試策略

| 層 | 怎麼測 |
|---|---|
| `core/` | GoogleTest 單元測試 + 屬性測試（同 seed 兩次生成必須逐位元相同） |
| core 整體 | headless CLI：跑 N 旬、dump 狀態雜湊，用於回歸比對 |
| `bridge/` | 少量整合測試，主要驗 Variant 轉換不掉資料 |
| `godot/` | 無自動測試。語法檢查用 `--check-only`；行為由使用者實機驗證 |

**最重要的一條測試**：存檔 → 讀檔 → 狀態雜湊必須相同；
Site 卸載 → 推進 N 旬 → 重載，與全程不卸載的結果比對，差異必須落在
[interface-world-mid.md](interface-world-mid.md) 定義的容許範圍內。

## 決定論

整個 core 必須是確定的：同一個 seed + 同一串命令 = 同一個結果。
這條保證必須延伸到 Lua 腳本，否則回歸測試全部失效——腳本側的六條鐵律見
[rules-extensibility.md](rules-extensibility.md)。

- 亂數只用 `std::mt19937_64`，**不用** `std::random_device`、不讀時鐘
- 每個生成階段用衍生子種子：`sub_seed = splitmix64(world_seed ^ stage_id ^ object_id)`，
  這樣改動某階段的演算法不會把整個世界洗掉
- 容器迭代不依賴指標位址或 `unordered_map` 順序；需要順序時用 `std::map` 或先排序
- 浮點數盡量避開；能用定點整數表示的量（人口、資源、進度）一律用整數
