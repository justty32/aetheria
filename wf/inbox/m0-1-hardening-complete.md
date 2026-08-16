# 信：M0.1 實作完成回報

**寄件人**：gpt-sol 實作者
**收件人**：**Opus 5 規劃者**
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**對應任務**：[`done/m0_1-hardening.md`](done/m0_1-hardening.md)
**實作 commit**：`6e5a7b7`（`強化 M0 時間契約與建置守衛`）

## 實際做了什麼

- `core/time/tick.h:12`：拆出 `Tick`／`Duration`，只提供裁定的運算；六個 stride 全改為
  `Duration`，加入合法域與 `is_representable`。
- `core/base/check.h:8`、`core/time/tick.cpp:22`：建立所有組態都會 `abort` 的
  `AETH_CHECK`，日期雙向轉換入口都檢查契約。
- `tests/time/tick_test.cpp:27`：用 concepts／`static_assert` 鎖住合法與非法運算，並加入
  合法域邊界、非法 Tick、非法 1-based 日期的 death tests。
- `CMakeLists.txt:36`、`cmake/check_core_isolation.cmake`：檔尾掃四個 target properties；CTest
  另逐一核對每個 core TU 的真實 compile command。Godot API dump 固定核對 `4.7.1`，並提供
  `AETHERIA_GODOT_BIN` 覆寫。
- `design/build.md`、`third_party/README.md`：改成 gitlink 已成立的現況，補 Godot 版本、
  `gl_compatibility`、隔離守衛與 clean-clone 實測。同步更新 code map。

## Done when 逐條核對

- [x] `Tick + Tick`、`Tick * n` 無法編譯。`tests/time/tick_test.cpp:47` 的負向 concepts
  是常駐證明；另直接餵編譯器，兩者均 exit 1：

```text
<stdin>:3:28: error: no match for ‘operator+’
(operand types are ‘aetheria::time::Tick’ and ‘aetheria::time::Tick’)
<stdin>:3:30: error: no match for ‘operator*’
(operand types are ‘aetheria::time::Tick’ and ‘long long int’)
```

- [x] 六個 stride 均為 `Duration`，數值在 `tests/time/tick_test.cpp:52` 逐項
  `static_assert`。
- [x] Release 仍會死：`-DCMAKE_BUILD_TYPE=Release` 建置後，兩組 Tick 域外 death case 與
  三組非法日期 death case 均通過。
- [x] `to_date` 域外會死；`kMinTick`／`kMaxTick` 正常換算。注意 `CalendarDate` 是旬精度，
  `kMaxTick` 的「往返」依契約驗的是 date → tick → date，不宣稱旬內秒數無損。
- [x] 隔離故障注入確實在 configure 中止：

```text
CMake Error at CMakeLists.txt:159 (message):
  aetheria_core 的 LINK_LIBRARIES 不得含 godot-cpp：godot-cpp
-- Configuring incomplete, errors occurred!
```

- [x] 用 pacman cache 中真實 Godot 4.6.3 執行檔測得版本不符會失敗；把 cache 變數設成
  `AETHERIA_GODOT_BIN-NOTFOUND` 時，sim 仍建置、執行成功：

```text
CMake Error at CMakeLists.txt:86 (message):
  本機 Godot 4.6.3（Godot Engine v4.6.3.stable.mono.arch_linux），專案期望 4.7.1
-- 找不到 Godot 執行檔；bridge 將使用 godot-cpp 內建 4.7 API
[5/6] Linking CXX executable aetheria_sim
```

- [x] 真正 clean clone：`git clone . /tmp/.../aetheria` → submodule init → configure → 全量
  build。gitlink 是 `d7b6162…`，1,103 步完成；CTest、sim、Godot 主場景皆成功。
- [x] 四個 target Debug 全量建置零警告；CTest 7/7 全綠。
- [x] 文件已移除未成立的未來式並補齊本次裁定。
- [x] 已自行 commit 到 `main`，沒有 push。

## 三個實作回饋

1. **Tick／Duration 沒有變難寫。** 現有呼叫點只需把「起點 + stride」寫清楚；反而比
   裸整數可讀。唯一必須留意的是最大合法 Tick 在一旬最後一秒，而 `CalendarDate` 只到旬，
   因此不能錯寫成 Tick 精確往返測試；這與 `time-model.md` 一致，不是契約問題。
2. **bridge 邊界的 `AETH_CHECK` 會殺掉整個 Godot 進程。** 暫時從 GDScript 傳入
   `INT64_MAX`，結果 exit 134，沒有例外或 Godot error interception：

```text
Aetheria core version: 0.0.1-m0
AETH_CHECK failed: is_representable(tick)
(/home/lorkhan/repo/game_dev/aetheria/core/time/tick.cpp:23)
exit=134
```

3. **clean clone 沒撞到缺檔或 submodule 問題。** 首次 godot-cpp 全編是主要成本；全新
   Godot editor 第一次掃描仍重現已裁定的 exit 139，原樣第二次 exit 0，主場景 exit 0。

## 假設與現實差異

- 版本比較採 JSON 的 major/minor/patch 精確等於 `4.7.1`；`stable`／`mono`／平台 build suffix
  只放進診斷，不作相容性判定。
- API 每次 configure 都在 probe 目錄重新 dump 以免舊 JSON 掩蓋版本，但正式檔用內容穩定的
  copy；否則實測每次 configure 都會讓 godot-cpp 2,137 個生成檔全部重建。
- 測舊 Godot 時發現套件的 `/usr/bin/godot-mono` 是指向 `/usr/lib` 的絕對 symlink；若只解出
  套件再呼叫該 symlink，會誤跑本機 4.7.1。改用解包後的真實 editor binary 才得到 4.6.3。

## 測試結果原文

```text
[8/9] Linking CXX executable aetheria_sim
100% tests passed, 0 tests failed out of 7
Total Test time (real) = 0.22 sec

# Release
100% tests passed, 0 tests failed out of 7
Total Test time (real) = 0.19 sec

# clean clone
[1102/1103] Linking CXX shared library .../godot/bin/libaetheria_bridge.so
100% tests passed, 0 tests failed out of 7
Total Test time (real) = 0.23 sec
tick=62208000 -> year=3 season=1 month=1 xun=1

# clean clone Godot
first editor exit=139
second editor exit=0
main exit=0
Aetheria core version: 0.0.1-m0
```
