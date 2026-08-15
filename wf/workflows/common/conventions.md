# 程式碼慣例（碼相關工作流共用）

← [common/README](README.md)｜[INDEX](../../INDEX.md)

碰原始碼的工作流（feature-dev / refactor / specs / plans…）共用這套規矩。純文檔/調查類工作流用不到。結構整理原則（被動、按需取用）在 [DEV-GUIDE](../../DEV-GUIDE.md)；always-on 鐵律在 [AGENTS.md](../../../AGENTS.md)。

## 程式碼慣例

- **核心 C++ 邏輯不得依賴 godot-cpp**：所有玩法邏輯（回合制策略、三層嵌套地圖、AI、規則判定）以純 C++ 實作，必須可獨立編譯與單元測試，不引用 godot-cpp / GDExtension API；只有 GDExtension 綁定層（暴露給 Godot 的介面）才碰 godot-cpp。
- **Godot 端不得持有玩法狀態**：GDScript / 場景樹只做顯示、美術、音效、UI、輸入轉發；任何玩法狀態（回合、單位、地圖資料）的唯一真相在 C++ 核心，Godot 端隨時可從核心重建畫面。

> 專案目前只在規劃階段，尚無程式碼骨架；檔案拆分方式、單檔行數門檻等細節待 `design/` 定案並開始實作後再補。

## 導航 index（code map）

專案目前無程式碼，尚未建立 code map。等 C++ 核心與 Godot 專案目錄長出來、agent 找檔開始有困難時，再依 [DEV-GUIDE「結構整理原則」](../../DEV-GUIDE.md) 建立。
