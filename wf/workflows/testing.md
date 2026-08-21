# testing — 跑測試（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

## 指令

- 首次 configure（`VCPKG_ROOT` 指向已 bootstrap 的 vcpkg）：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

- **快速驗證**（純 C++，不需 Godot）：

```sh
cmake --build build --target aetheria_tests aetheria_sim --parallel
ctest --test-dir build --output-on-failure
./build/aetheria_sim --tick 62208000
```

- **完整驗證**：

```sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/aetheria_sim --tick 62208000
godot-mono --headless --path godot --editor --quit-after 3
godot-mono --headless --path godot --quit-after 5
```

Godot 全新專案第一次 editor 掃描可能 exit 139；若掃描已完成，原樣重跑一次。第二次與主場景
都必須 exit 0，不能因已知首掃問題略過後兩項。完整建置說明見 [design/build.md](../../design/build.md)。

## 測試分類

- `unit`：`aetheria_tests`，GoogleTest 經 CTest 發現；純 C++、不需 Godot。
- `headless`：`aetheria_sim`，驗證 core 可獨立執行與穩定文字輸出。
- `integration`：Godot headless editor + 主場景，驗證 GDExtension 註冊與 Variant 轉換。

## 效能斷言

共用機器上的競爭只會把 wall-clock 樣本拉高。所有效能測試必須先暖機，再固定量至少 5 次
並以最小值斷言；不得用單次、平均值或「重跑到通過」。固定樣本數同時守住測試時間上界。
共用 helper 在 `tests/support/performance.h`。

跑不了的環境依賴驗證 → 記 [WAIT_USER](../WAIT_USER.md)。
