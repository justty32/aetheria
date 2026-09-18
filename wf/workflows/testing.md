# testing — 跑測試 / 驗證

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

改完怎麼確認沒壞：有哪些驗證、各自的指令、哪些 agent 自己跑得了、哪些得交給使用者。

**何時用**：改完程式要驗、使用者說「跑測試」、重構前要記基準、驗收實作回報。
**何時不用**：環境 configure 不過 → 先看 [dispatch/OPS-NOTES](dispatch/OPS-NOTES.md) 的冷啟動配方與 [design/architecture/build.md](../../design/architecture/build.md)；驗證紅了要查成因 → [investigation](investigation/README.md)。

## Done when

- 「快速驗證」三條指令回傳 0，`ctest` 測試數與基準相同（門檻要從組成推導，不寫拍腦袋的整數）。
- 要實機／人眼／真素材的那類，已在 [WAIT_USER](../WAIT_USER.md) 各留一行。

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
都必須 exit 0，不能因已知首掃問題略過後兩項。完整建置說明見 [design/architecture/build.md](../../design/architecture/build.md)。

## 測試分類

- `unit`：`aetheria_tests`，GoogleTest 經 CTest 發現；純 C++、不需 Godot。
- `headless`：`aetheria_sim`，驗證 core 可獨立執行與穩定文字輸出。
- `integration`：Godot headless editor + 主場景，驗證 GDExtension 註冊與 Variant 轉換。

## 效能斷言

共用機器上的競爭只會把 wall-clock 樣本拉高。所有效能測試必須先暖機，再固定量至少 5 次
並以最小值斷言；不得用單次、平均值或「重跑到通過」。固定樣本數同時守住測試時間上界。
共用 helper 在 `tests/support/performance.h`。

## 綠燈不等於有檢查

**一道檢查通過，可能是因為它根本沒在檢查。** 本專案抓過的實例：驗收腳本印的「跨行程可重現」雜湊用的是沒有平手的佔位色盤，注入錯誤也不變（M9.2）；三層期望一致只守 `|誤差|<5%`，Site 系統性 +3% 全綠（M7.2）。

**規則：新增或修改一道檢查時，要證明它能變紅**——先餵一個應該被擋的輸入確認 exit ≠ 0，再餵正確輸入確認 exit = 0；驗收回報時**自己重跑關鍵的負向控制**，只讀回報會簽收。方法與實例見 [dispatch/OPS-VERIFY](dispatch/OPS-VERIFY.md)，已知的假通過陷阱見 [KNOWN-TRAPS](../KNOWN-TRAPS.md)。靜態全過不等於畫面上是對的——方框、亂碼、切層穿幫只有人眼看得出來，記到 [WAIT_USER](../WAIT_USER.md)，不要自己宣稱通過。

## 誰跑

只有兩種值：**agent**（本機跑得動：unit／headless／integration 都是）、**使用者 → WAIT_USER**（要實機、真素材、帳號、目視或耳聽）。判不準就當後者；跑不了的環境依賴驗證 → 記 [WAIT_USER](../WAIT_USER.md)。

## 交接

- 綠燈後回 [feature-dev](feature-dev/README.md) 接完剩下的步驟（code map → 文檔 → commit）。
- 同一個紅燈第二次撞到 → [common/gotchas](common/gotchas.md)；會在未來咬人的 → [KNOWN-TRAPS](../KNOWN-TRAPS.md)。
