# code-map — 原始碼導航

← [common/README](README.md)｜[conventions](conventions.md)｜[INDEX](../../INDEX.md)

**目錄＝職責，檔名＝子題。** 本頁派發到領域詳圖；結構維護順序見 [refactor](../refactor.md)。

## 領域入口

<!-- wf-nav -->
- [Runtime、跨 zone 與歷史](code-map-runtime.md)
- [Rules 與資料載入](code-map-rules.md)
- [Zone 與序列化](code-map-persistence.md)
- [Region、AI 與 observer](code-map-world.md)
- [Site 投影、城建與駐留](code-map-site.md)
- [Local 與共用 spatial 演算法](code-map-local.md)
- [Narrative 與 Script](code-map-narrative-script.md)
- [Worldgen 十二階段生成](code-map-worldgen.md)
- [Bridge、Godot 與 Sim](code-map-adapters.md)
- [測試目錄與護欄](code-map-tests.md)

## 共用檔名讀法

同名 `.h` 與 `.cpp` 通常是介面與實作；同前綴加職責後綴的 `.cpp` 是同一模組的分檔，
應先找公開 header，再按職責讀實作。`*_detail.h` 是同目錄跨檔共用 helper，
單檔 helper 留匿名 namespace；`gen_*.h` 是 worldgen 的內部共用工具。
`*_test_support.h` 是該測試目錄專用的 fixture／helper，跨目錄共用者放測試 support 目錄。

門面保留原 include 入口。純彙整門面只 include 子 header；需要維持 enum 定義路徑的門面
仍保留列舉，PlayableSession 入口另保留報告型別，不能一概視為空殼。
`*_types.h`、`*_impl.h` 分別容納型別與 header 內實作；`.inc` 是在指定位置文字引入，
不代表可獨立 include 的公開 header。

## 頂層職責

`core/` 是純 C++ 玩法核心，不得依賴 godot-cpp；`bridge/` 是 GDExtension 轉接；
`godot/` 只負責顯示、美術、音效、UI 與輸入轉發，玩法狀態唯一真相在核心。
`sim/` 提供 headless CLI，`tests/` 提供 GoogleTest 與護欄驗證，`data/` 保存 TOML def 與規則。
`third_party/godot-cpp/` 是固定 commit 的 submodule。

`cmake/` 管 target、工具鏈與 CTest 檢查；來源清單在 `cmake/targets_*.cmake`，
入口是 `cmake/targets_core.cmake`／`cmake/targets_tests.cmake`。
頂層 CMake 組態與 vcpkg manifest 負責專案設定及依賴。

## 小型核心目錄

`core/base/` 提供不變式檢查；`core/api/` 提供核心版本介面；
`core/time/` 提供 Tick、Duration 與 360 天曆換算。各目錄以同名 header／實作配對，
沒有玩法編排責任。
