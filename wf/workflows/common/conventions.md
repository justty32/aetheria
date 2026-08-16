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

原始碼導航拆到獨立一檔：**[code-map.md](code-map.md)**（目錄職責、檔名慣例、每個領域有哪些檔）。
拆檔／重構後回去更新它，維護鏈見 [refactor](../refactor.md)。
