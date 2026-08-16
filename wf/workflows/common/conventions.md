# 程式碼慣例（碼相關工作流共用）

← [common/README](README.md)｜[INDEX](../../INDEX.md)

碰原始碼的工作流（feature-dev / refactor / specs / plans…）共用這套規矩。純文檔/調查類工作流用不到。結構整理原則（被動、按需取用）在 [DEV-GUIDE](../../DEV-GUIDE.md)；always-on 鐵律在 [AGENTS.md](../../../AGENTS.md)。

## 程式碼慣例

- **核心 C++ 邏輯不得依賴 godot-cpp**：所有玩法邏輯（回合制策略、三層嵌套地圖、AI、規則判定）以純 C++ 實作，必須可獨立編譯與單元測試，不引用 godot-cpp / GDExtension API；只有 GDExtension 綁定層（暴露給 Godot 的介面）才碰 godot-cpp。
- **Godot 端不得持有玩法狀態**：GDScript / 場景樹只做顯示、美術、音效、UI、輸入轉發；任何玩法狀態（回合、單位、地圖資料）的唯一真相在 C++ 核心，Godot 端隨時可從核心重建畫面。
- **新增可存檔 component 必須同步登記 `core/serialize/all_components.h` 的 `AllComponents`，且永遠加在清單尾端**：這份順序就是 EnTT snapshot 的位元流順序；漏登記會靜默漏存，插在中間會讓舊存檔錯位。

建置與工具鏈細節見 [design/build.md](../../../design/build.md)；驗證指令見
[testing](../testing.md)。所有自有 C++ target 以 C++23、`-Wall -Wextra -Wpedantic -Werror`
（MSVC `/W4 /WX`）建置。第三方 header 以 `SYSTEM` include 隔離，但不得關閉自有警告。

## 導航 index（code map）

M0 的檔案量仍小，本節直接兼作 code map；長大後再依
[DEV-GUIDE「結構整理原則」](../../DEV-GUIDE.md) 拆成獨立 index。

| 路徑 | 職責 |
|---|---|
| `CMakeLists.txt`、`vcpkg.json` | 四 target、隔離檢查、依賴固定與 Godot API dump |
| `cmake/` | CTest 使用的 core 編譯命令隔離檢查 |
| `core/base/` | 所有建置組態都生效的 core 不變式檢查 |
| `core/time/` | 純 C++ Tick／Duration 與 360 天曆換算 |
| `core/rules/`、`data/` | 不可變 Ruleset、TOML def 載入與四類基礎定義檔 |
| `core/serialize/` | PortableBinary canonical zone 位元流、EnTT snapshot 與 `AllComponents` |
| `core/zone/` | ZoneKey、Zone、記憶體／zstd 磁碟 store 與 ZoneManager 生命週期 |
| `core/world/` | L1 `RegionTiles` SoA、四鄰接座標與雙邊一致 edge 寫入 |
| `core/worldgen/` | 純 C++ Region 生成骨架、階段子種子、板塊／高度／侵蝕與唯一量化閘口 |
| `core/api/` | core 對外 API；目前只有版本 |
| `tests/time/` | 曆法邊界與往返 GoogleTest |
| `tests/zone/` | ZoneKey、生命週期、兩種 store 共用契約、損毀拒讀與 round-trip 測試 |
| `tests/rules/`、`tests/world/`、`tests/serialize/` | Ruleset fail-fast／id 重映射、SoA edge／記憶體與 EnTT 壓測 |
| `tests/worldgen/` | Region 生成決定論、階段隔離、量化型別閘口、連通性與效能預算 |
| `sim/` | 不需 Godot 的曆法與 zone 樹 CLI 探針 |
| `bridge/` | `AetheriaCore` Node 與 GDExtension 註冊；唯一可 include godot-cpp 的自有目錄 |
| `godot/` | 純顯示／呼叫驗證場景與 `.gdextension` 描述檔 |
| `third_party/godot-cpp/` | 固定 commit 的 submodule checkout |
