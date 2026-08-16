# 信：補充 M0 做了什麼、改了什麼

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**回覆哪封**：`m0-bootstrap.md`；補充 `m0-bootstrap-complete.md`

這封只補「實際變更清單」，讓審閱時不用先翻完整 diff。

## 新增：建置與依賴

| 檔案／路徑 | 做了什麼 |
|---|---|
| `CMakeLists.txt` | 新建四個 target；統一 C++23；自有程式開 `-Wall -Wextra -Wpedantic -Werror`；配置 CTest、vcpkg、Godot API dump、godot-cpp HEAD 核對；configure 時禁止 core target 出現 godot-cpp include。 |
| `vcpkg.json` | 新建 manifest，只加入 `gtest`、`cli11`，baseline 固定 `b781af668027bbf77f2f827f47b5c6cd8d825c08`。 |
| `.gitmodules` | 登記 `third_party/godot-cpp` 的官方來源。 |
| `third_party/godot-cpp/` | 放入 detached checkout，固定 `d7b6162249ed52796a8301d216c24ee71d68c2bf`；沒有改上游原始碼。 |
| `third_party/README.md` | 記錄來源、commit 與提交 M0 時要正式登記 gitlink。 |

godot-cpp 上游 target 寫死 C++17，所以在 `add_subdirectory` 後覆寫成 C++23；同時開啟例外、
關閉 static libstdc++，使 core／bridge／godot-cpp 的編譯器、標準與 ABI 旗標一致。

## 新增：純 C++ core

| 檔案 | 做了什麼 |
|---|---|
| `core/time/tick.h` | 新增 `Tick : int64_t`、`kSecondsPerXun`、`CalendarDate`、`to_date()`、`to_tick()` 公開介面。 |
| `core/time/tick.cpp` | 實作 4 季 × 3 月 × 3 旬的 360 天曆；Tick 0 是第 1 年第 1 季第 1 月第 1 旬；日期 1-based；負值用 floor division。 |
| `core/api/version.h/.cpp` | 新增 `core_version()`，目前回傳 `0.0.1-m0`。 |

core 沒有 include 或 link 任何 Godot／godot-cpp；編譯命令只有 repo 根 include。

## 新增：測試與 headless CLI

| 檔案 | 做了什麼 |
|---|---|
| `tests/time/tick_test.cpp` | 新增曆法 epoch／旬月季年邊界測試，以及代表年份內每一旬的 date→tick→date 往返測試。 |
| `sim/main.cpp` | 新增 CLI11 程式，印 core 版本與三筆 Tick 換算；`--tick` 可指定額外樣本。 |

另外做過 mutation check：暫時把每月 3 旬改成 4 旬，邊界測試確實失敗；之後已還原並重跑 2/2 綠。

## 新增：GDExtension 與 Godot 專案

| 檔案 | 做了什麼 |
|---|---|
| `bridge/aetheria_core.h/.cpp` | 新增 Godot `Node` 類別 `AetheriaCore`；暴露 `get_core_version()` 與 `tick_to_date(int64) -> Dictionary`。 |
| `bridge/register_types.h/.cpp` | 新增 GDExtension 初始化入口 `aetheria_library_init`，在 scene level 註冊類別。 |
| `godot/project.godot` | 新建 Godot 4.7 專案，主場景指向 `main.tscn`。 |
| `godot/aetheria.gdextension` | 新增 Linux／Windows／macOS 動態庫路徑與 entry symbol。 |
| `godot/main.gd`、`main.tscn` | `_ready()` 建立 `AetheriaCore`，印版本、Tick 0 與一年後日期，再退出；沒有保存玩法狀態。 |
| `godot/*.uid` | Godot 4.7.1 editor 掃描自動產生的資源 UID。 |

`aetheria_bridge` 建完直接輸出到 `godot/bin/`；`.so` 與 `.godot/` 都由既有 `.gitignore` 排除。

## 新增／修改：文件與工作流

| 檔案 | 改了什麼 |
|---|---|
| `design/build.md` | 新增繁中 M0 建置手冊：實際版本、指令、產物、驗證、可重現性與四個踩坑。 |
| `design/README.md` | 把 `build.md` 加進設計索引。 |
| `README.md` | 現況從「只有規劃」改成「M0 骨架已落地」；補建置入口與新目錄。 |
| `AGENTS.md` | 專案現況改成 M0 已建立；build/test 連到實際工作流。 |
| `wf/INDEX.md` | 頂層地圖加入 CMake、core、bridge、godot、tests、sim、third_party。 |
| `wf/workflows/testing.md` | 補首次 configure、快速驗證、完整驗證與三類測試。 |
| `wf/workflows/common/conventions.md` | 補 C++23／warning 契約與 M0 code map。 |
| `wf/workflows/feature-dev/README.md` | 移除「尚無程式碼骨架」舊說法，改連 testing／code map。 |
| `wf/SESSION-LOG.md` | 依 open-only 規約移除已完成的規劃／M0 等條目；保留真正未解的設計缺口。 |
| `wf/WAIT_USER.md` | 移除已失效的「CONTACTS 空白」與已裁定歷史，目前無待使用者項。 |
| `wf/inbox/done/` | 把 `implementer-onboarding.md`、`m0-bootstrap.md` 移入 done。 |

## 沒有做

- 沒有實作 Zone、生成器、EnTT、序列化、Lua 或任何玩法。
- 沒有修改既有設計結論；只有新增實際 build 文件與索引／狀態同步。
- 沒有 `git init`、stage、commit 或 push；既有 `.gitignore` 最終未改。

## 最終驗證

```text
自有來源重編：10 steps，零 warning（warning 已視為 error）
CTest：2/2 passed
aetheria_sim：exit 0
Godot 4.7.1 headless editor：exit 0
Godot 主場景：exit 0，版本與兩筆曆法輸出正確
```
