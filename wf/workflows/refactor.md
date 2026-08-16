# refactor — 重構 / 拆檔 / 整理結構（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

觸發：「幫忙拆檔」「這個檔太大了」「整理一下結構」。**只搬不改**——重構的定義就是行為不變。

碰原始碼時的程式碼慣例見 [common/conventions](common/conventions.md)，結構整理原則（膨脹即拆／雜亂即分類、四級成長軌跡）見 [DEV-GUIDE](../DEV-GUIDE.md)。本檔只寫**怎麼跑一輪拆檔**。

## Done when

- 目標檔案全部 ≤ 8192 bytes（`git ls-files | xargs ls -l` 掃一遍確認）。
- 每個新檔有**單一清楚職責**，檔名一眼看得出來。
- `ctest` 測試數與拆檔前**完全相同**、全數通過；`aetheria_sim` 輸出不變。
- [common/conventions](common/conventions.md) 的 code map 已更新（見下方「維護鏈」）。

不包含：改演算法、改介面、補測試、改錯誤訊息文字。發現真的該改的東西 → 另開 feature-dev，不要夾帶。

## 流程

1. **先量再拆**。列出超標檔案：

   ```sh
   git ls-files | xargs ls -l 2>/dev/null | awk '$5>8192 {printf "%8d  %s\n",$5,$9}' | sort -rn
   ```

   注意 `git ls-files` 會列出 submodule 目錄本身，`ls -l` 會展開它的內容——`third_party/godot-cpp` 底下的檔案不是我們的，忽略。

2. **存基準線**：拆之前先跑一次完整驗證（見 [testing](testing.md)），記下測試數。基準線不綠就先別拆。

3. **按語意切，不按行數切**。一個 `.cpp` 通常照「一個階段／一個職責一檔」切；匿名 namespace 裡**跨檔共用**的 helper 才提成內部 header（`*_detail.h` / `gen_*.h`，放 `detail` sub-namespace），**只被單一檔用到的一律留在該檔的匿名 namespace**。

4. **大 header 留成門面**。被很多地方 include 的 header（`region_generator.h`、`ruleset.h`）拆完後**保留原檔名**，內容只剩 `#include` 一串新 header——呼叫端一行都不用改，diff 才審得動。

5. **私有成員要拆到別的 TU**：自由函式碰不到 private，要在 class 裡加 private static 成員函式再分檔定義（`RulesetLoader` 就是這樣拆的）。

6. **更新建置清單**：`cmake/targets_*.cmake` 的來源清單（不是頂層 `CMakeLists.txt`）。

7. **驗證**：完整跑 [testing](testing.md)。測試數必須與基準線相同。

## 派工（多個檔案平行拆）

拆檔是機械工，適合開 subagent 平行做。**唯一的協作陷阱是建置清單**：多個 agent 同時改 `cmake/targets_*.cmake` 會打架。規約：

- **agent 不准碰 `cmake/`**，只回報「新增／刪除了哪些 `.cpp`」，由協調者統一更新。
- agent 各自用 `-fsyntax-only` 自我驗證，不需要完整建置就能確認自己那幾個 TU 是乾淨的：

  ```sh
  c++ -I. -isystem build/vcpkg_installed/x64-linux/include -std=c++23 \
      -Wall -Wextra -Wpedantic -Werror -DTOML_HEADER_ONLY=0 -fsyntax-only <檔案>
  ```

  （`sim/main.cpp` 另需 `-DAETHERIA_DEFAULT_DATA_DIR="data"`，該巨集平常由 CMake 注入。）
- 每個 agent 分到的檔案集合必須**互斥**；會被別人改寫的 header 只准 include、不准改。
- 協調者最後做一次完整建置 + `ctest`，link error 由協調者收尾。

## 維護鏈（拆完一定要做）

拆檔會讓 code map 過期。更新順序：

1. [common/conventions](common/conventions.md) 的 code map 表格——**以目錄為單位**描述職責，不逐檔列，這樣下次拆檔不必再改它。
2. 若某目錄的檔案多到 code map 一行講不清，才在該目錄開自己的 README。
3. [SESSION-LOG](../SESSION-LOG.md) 只留還沒收尾的 open 項，拆完就刪除。
