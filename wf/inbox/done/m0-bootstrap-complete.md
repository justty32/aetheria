# 信：M0 可編譯骨架完成回報

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**回覆哪封**：`m0-bootstrap.md`

## 實際完成

- `CMakeLists.txt`、`vcpkg.json`、`.gitmodules`、`third_party/`：四個 target、vcpkg baseline、
  godot-cpp 固定 commit／HEAD 核對、本機 Godot API dump、C++23 與警告契約。
- `core/time/`、`core/api/`：強型別 Tick、360 天曆雙向換算、core 版本。
- `tests/time/`、`sim/`：兩個 GoogleTest 與 CLI11 headless 探針。
- `bridge/`、`godot/`：`AetheriaCore` Node、兩個方法、GDExtension 註冊與驗證場景。
- `design/build.md`：實際建置、固定點、驗證與踩坑；並同步 README、testing、conventions code map、
  repo index 與 open-only 活狀態。

## Done when 核對

- [x] 一次 `cmake --build` 產出 `aetheria_core`、`aetheria_tests`、`aetheria_sim`、
  `aetheria_bridge`。自有 target 為 `-Wall -Wextra -Wpedantic -Werror`，最終編譯零警告。
- [x] CTest 2/2 綠。暫改 `kXunPerMonth` 3→4 後邊界測試確實失敗，再還原並重跑綠。
- [x] sim 不需 Godot，`ldd` 只有 libc／libstdc++／libm／libgcc；輸出曆法正確。
- [x] Godot 4.7.1 headless editor 載入成功，主場景 exit 0 並印出版本／曆法。
- [x] core target 沒有 godot-cpp include／link；CMake configure 另有 fail-fast 檢查。
- [x] `vcpkg.json` 固定 baseline；godot-cpp 固定 commit 且 CMake 核對 HEAD。因本次變更依指示
  不 commit，尚無含 M0 的 commit 可做實際 clean clone；已完成相依鏈論證，commit 後仍需補實測。
- [x] `design/build.md` 已寫，4,300 bytes 內，繁體中文。

## godot-cpp C++23 實況

- 本機 Godot：`4.7.1.stable.mono.arch_linux.a13da4feb`。
- godot-cpp：官方 master 固定 `d7b6162249ed52796a8301d216c24ee71d68c2bf`；它內建 4.7 API，
  實際 configure 仍由 `/usr/bin/godot-mono --headless --dump-extension-api` 產生 4.7.1 API。
- 上游 CMake 在 `cmake/godotcpp.cmake` 把 target 寫死 `CXX_STANDARD 17`。沒有改上游檔；
  `add_subdirectory` 後用 target property 改成 23，並把 `GODOTCPP_DISABLE_EXCEPTIONS=OFF`、
  `GODOTCPP_USE_STATIC_CPP=OFF`，讓 core／bridge／godot-cpp 用同一 GCC、C++23、例外與 runtime ABI。
- 首次 configure 約 21 秒（產生 2,137 個 binding 檔），12-way 首編約 1 分鐘。
- bridge 的 `-Wall -Wextra` 起初揭露大量上游 header unused-parameter；改用 godot-cpp 官方
  `GODOTCPP_SYSTEM_HEADERS=ON` 隔離第三方 header，自有警告仍全開並升成 error。

## 自行決定的細節

- Tick 0 = 第 1 年第 1 季第 1 月第 1 旬起點；日期欄位 1-based。
- 新增 `to_tick(CalendarDate)` 供往返測試；代表該旬第一秒。負年份採 proleptic 360 天曆。
- core 版本先定 `0.0.1-m0`；bridge 直接輸出 `Dictionary{year,season,month,xun}`。
- Godot 專案以已安裝的 4.7.1 為準；bridge 直接輸出到 `godot/bin/`。
- godot-cpp public headers 標為 `SYSTEM`，不讓第三方診斷掩蓋自有程式的零警告契約。

## 設計與現實不符

1. 任務書與 `AGENTS.md` 稱 repo 尚未 `git init`，但實作末段核對得到：

```text
$ git rev-parse --show-toplevel
/home/lorkhan/repo/game_dev/aetheria
$ git log -1 --oneline
6ab8f19 建立 aetheria 專案：設計文件體系與工作流
```

   我沒有 init、stage、commit 或 push。godot-cpp 現為 detached checkout + `.gitmodules`；M0 要提交時
   需把它正式登記成 gitlink。
2. `Tick` 的 `int64_t` 秒域可到約 2.9×10¹¹ 年，但任務書指定 `CalendarDate.year: int32_t`，只能表示
   約 ±2.1×10⁹ 年。現況依任務書實作；超出 int32 年域的 `to_date` 不具可表達性，請裁定是縮小
   Tick 的有效契約、把 year 改 int64，或讓換算回傳錯誤。
3. 即使用較新的 godot-cpp commit 與本機 4.7.1 API，**全新** `.godot/` 第一次 headless editor
   掃描仍在完成 filesystem scan 後 exit 139；不改檔第二次 exit 0，之後場景穩定 exit 0。
   這和 medps 記錄相同，但不能再歸因於「舊 godot-cpp checkout」；較像 Godot Mono 首次初始化問題。

## 測試結果原文

```text
[1/11] Building CXX object CMakeFiles/aetheria_core.dir/core/api/version.cpp.o
...
[10/11] Linking CXX executable aetheria_sim
```

```text
Test project /home/lorkhan/repo/game_dev/aetheria/build
1/2 Test #1: CalendarConversion.ConvertsEpochAndCalendarBoundaries ........   Passed
2/2 Test #2: CalendarConversion.RoundTripsEveryXunInRepresentativeYears ...   Passed
100% tests passed out of 2
```

Mutation check：

```text
1/2 Test #1: CalendarConversion.ConvertsEpochAndCalendarBoundaries ........***Failed
2/2 Test #2: CalendarConversion.RoundTripsEveryXunInRepresentativeYears ...   Passed
50% tests passed, 1 tests failed out of 2
```

```text
Aetheria core 0.0.1-m0
tick=0 -> year=1 season=1 month=1 xun=1
tick=864000 -> year=1 season=1 month=1 xun=2
tick=62208000 -> year=3 season=1 month=1 xun=1
```

```text
Godot Engine v4.7.1.stable.mono.arch_linux.a13da4feb
Aetheria core version: 0.0.1-m0
Tick 0: { "year": 1, "season": 1, "month": 1, "xun": 1 }
Tick 31104000: { "year": 2, "season": 1, "month": 1, "xun": 1 }
```

core 編譯命令的關鍵段：

```text
/usr/bin/c++ -I/home/lorkhan/repo/game_dev/aetheria -std=c++23 -Wall -Wextra -Wpedantic -Werror
```

bridge 才另外出現兩條 godot-cpp `-isystem` include。
