# 程式碼慣例（碼相關工作流共用）

← [common/README](README.md)｜[INDEX](../../INDEX.md)

碰原始碼的工作流（feature-dev / refactor / specs / plans…）共用這套規矩。純文檔/調查類工作流用不到。結構整理原則（被動、按需取用）在 [DEV-GUIDE](../../DEV-GUIDE.md)；always-on 鐵律在 [AGENTS.md](../../../AGENTS.md)。

## 程式碼慣例

- **核心 C++ 邏輯不得依賴 godot-cpp**：所有玩法邏輯（回合制策略、三層嵌套地圖、AI、規則判定）以純 C++ 實作，必須可獨立編譯與單元測試，不引用 godot-cpp / GDExtension API；只有 GDExtension 綁定層（暴露給 Godot 的介面）才碰 godot-cpp。
- **Godot 端不得持有玩法狀態**：GDScript / 場景樹只做顯示、美術、音效、UI、輸入轉發；任何玩法狀態（回合、單位、地圖資料）的唯一真相在 C++ 核心，Godot 端隨時可從核心重建畫面。

建置與工具鏈細節見 [design/build.md](../../../design/build.md)；驗證指令見
[testing](../testing.md)。所有自有 C++ target 以 C++23、`-Wall -Wextra -Wpedantic -Werror`
（MSVC `/W4 /WX`）建置。第三方 header 以 `SYSTEM` include 隔離，但不得關閉自有警告。

## 導航 index（code map）

M0 的檔案量仍小，本節直接兼作 code map；長大後再依
[DEV-GUIDE「結構整理原則」](../../DEV-GUIDE.md) 拆成獨立 index。

| 路徑 | 職責 |
|---|---|
| `CMakeLists.txt`、`vcpkg.json` | 四 target、隔離檢查、依賴固定與 Godot API dump |
| `core/time/` | 純 C++ Tick 與 360 天曆換算 |
| `core/api/` | core 對外 API；目前只有版本 |
| `tests/time/` | 曆法邊界與往返 GoogleTest |
| `sim/` | 不需 Godot 的 CLI 探針 |
| `bridge/` | `AetheriaCore` Node 與 GDExtension 註冊；唯一可 include godot-cpp 的自有目錄 |
| `godot/` | 純顯示／呼叫驗證場景與 `.gdextension` 描述檔 |
| `third_party/godot-cpp/` | 固定 commit 的 submodule checkout |
