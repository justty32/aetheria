# 建置與工具鏈

> 承接 [tech-stack.md](tech-stack.md) 與 [cpp-conventions.md](cpp-conventions.md)。
> 本文件記錄 M0 實際跑通的建置方式；玩法設計以 [principles.md](principles.md) 為準，
> 用詞以 [glossary.md](glossary.md) 為準。

## 已驗證環境

| 元件 | 版本／固定點 |
|---|---|
| OS | Manjaro Linux x86_64 |
| CMake / generator | 4.4.2 / Ninja 1.13.2 |
| C++ | GCC 16.1.1，C++23、關閉 GNU extensions |
| Godot | `godot-mono 4.7.1.stable`；GDExtension API 期望版本固定為 `4.7.1` |
| vcpkg baseline | `b781af668027bbf77f2f827f47b5c6cd8d825c08` |
| godot-cpp | submodule commit `d7b6162249ed52796a8301d216c24ee71d68c2bf` |

vcpkg manifest 只含 `gtest` 與 `cli11`。godot-cpp 不走 vcpkg，來源與固定點另見
[`third_party/README.md`](../third_party/README.md)。

## 從乾淨 checkout 建置

需要 CMake 3.25+、Ninja、支援 C++23 的編譯器、Git、Python 3 與一份已 bootstrap 的 vcpkg。
Godot 不是編譯 `aetheria_sim` 的必要條件；要做 GDExtension 執行期驗證才需要。

```sh
git submodule update --init --recursive
export VCPKG_ROOT=/path/to/vcpkg
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --parallel
```

第一次 configure 會依 `vcpkg.json` 的 baseline 安裝依賴。若找到 `godot`、`godot4`、
`godot-4` 或 `godot-mono`，CMake 會執行 `--dump-extension-api`，並核對 JSON header 必須是
Godot `4.7.1`；不符就中止 configure。可用 cache 變數
`-DAETHERIA_GODOT_BIN=/path/to/godot` 明確覆寫執行檔。找不到 Godot 時改用 godot-cpp
內建的 4.7 API，`aetheria_sim` 仍可獨立建置。

產物：

| target | 產物 |
|---|---|
| `aetheria_core` | `build/libaetheria_core.a` |
| `aetheria_tests` | `build/aetheria_tests` |
| `aetheria_sim` | `build/aetheria_sim` |
| `aetheria_bridge` | `godot/bin/libaetheria_bridge.so`（其他平台為 `.dll`／`.dylib`） |

## 驗證

```sh
ctest --test-dir build --output-on-failure
./build/aetheria_sim --tick 62208000
godot-mono --headless --path godot --editor --quit-after 3
godot-mono --headless --path godot --quit-after 5
```

最後一行應印出 `0.0.1-m0`，以及 Tick 0 與 31,104,000 的日期。

四個 Aetheria target 都開 `-Wall -Wextra -Wpedantic -Werror`（MSVC 為 `/W4 /WX`）。
`CMAKE_CXX_STANDARD=23` 套用全專案；godot-cpp 上游仍預設 C++17，因此加入 subdirectory 後
另以 target property 強制為 C++23。exception ABI 與標準函式庫連結也明確改成跟專案一致。

`aetheria_core` 只公開 repo 根 include，CMake configure 會掃 target include properties，
也掃直接／介面 link properties；一旦出現 `godot-cpp`／`godot_cpp` 就 fail-fast。
CTest 的 `CoreIsolation.CompileCommands` 另掃 `build/compile_commands.json` 中每一個 core TU，
驗證傳遞展開後的實際編譯命令也沒有 godot-cpp。bridge 才有第三方 `-isystem`；
`cmake --graphviz=...` 也顯示只有 bridge 連 godot-cpp，sim 僅連 core + CLI11。

Godot 專案固定使用 `gl_compatibility` renderer；M0 的 headless 探針與未來 2D 顯示均以這條
相容性路徑為基準，不在不同開發機上自動漂移 renderer。

## 可重現性論證

- vcpkg manifest 固定 baseline，套件版本由同一份 ports tree 決定。
- godot-cpp 已由 submodule gitlink 固定 commit；CMake 也核對 checkout HEAD。
- 編譯器與 Godot 屬外部工具鏈，版本應由 CI image 或開發環境文件固定。

<!-- M0.1 commit 後在乾淨 clone 重跑，將實測結果補在這裡。 -->

## 實際踩坑

1. 本機執行檔叫 `godot-mono`，只找 `godot`／`godot4` 會誤判未安裝。
2. godot-cpp CMake 把自身 target 寫死為 C++17；只設全域 `CMAKE_CXX_STANDARD` 不夠，必須在
   `add_subdirectory` 後覆寫 target property。M0 首次 configure 約 21 秒，12-way 首編約 1 分鐘。
3. bridge 開 `-Wall -Wextra` 時，上游巨集 header 會產生大量 unused-parameter 警告；將第三方
   public headers 標成 CMake `SYSTEM` 後，本專案 warning 仍全開且能保持零警告。
4. Godot 4.7.1 Mono 對全新 `godot/.godot/` 的第一次 headless editor 掃描，在完成檔案掃描後
   曾以 139 結束；不改檔直接再跑即為 0，之後主場景也穩定為 0。這是一次性初始化坑，不能把
   第一次失敗當成 bridge 載入失敗；仍應檢查第二次與主場景的原始輸出。
5. 每次 configure 直接覆寫正式 `extension_api.json`，即使內容相同也會令 godot-cpp 的 2,137
   個生成檔重新產生。現在先 dump 到 build 內的 probe 目錄核對版本，再用 content-stable copy
   更新正式檔；重跑 configure 後 Ninja 可維持 `no work to do`。
