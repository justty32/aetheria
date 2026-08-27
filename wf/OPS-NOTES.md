# OPS-NOTES — 操作心得（派工、限流、通訊）

← [SESSION-LOG.md](SESSION-LOG.md)｜[AGENTS.md](../AGENTS.md)｜**團隊角色與節奏 → [TEAM.md](TEAM.md)**

> 與**進度**無關、但**每次派工都會用到**的東西。
> 這裡的每一條都是踩過才寫下來的，不是推測。

- 🔧 **派 codex**：`-c model_reasoning_effort="high"` 覆寫 config 的 `ultra`（60 分 → 12 分，品質沒掉）；
  prompt 要明令**不准 fan-out 自我審查子 agent**、**`--parallel 2`**（不帶數字會開滿核心）。
  並行用 `git worktree`，但要挑**檔案真正互斥**的兩件事。
- 🔧 **限流的歸屬判斷走「路徑」，不走名稱、也不走行程樹**（前兩種昨天都試過都錯）。
  按名稱會打到別的 agent 的同名實例（`cc1plus`／`ld`／`housecarl-mcp`，
  Skyrim agent 實測同時有 **10 份** houseCARL）。
  ⚠ **按行程樹也錯**：`nohup ... &` 派的 codex 會被 reparent 到 systemd，
  往上找 claude 那條鏈**會斷**——後果是**自己的 build 從來沒被限流**，
  而且它會建議「寄信指控對方」**而那些負載是自己的**。
  ⚠ **「看 `cwd` 是否在 scratchpad 底下」也錯**（2026-08-22 實測）：
  `codex exec -C <dir>` 的 `-C` 只改 codex **內部**的工作目錄，
  **行程 cwd 仍是啟動它的地方**（我從 repo 根派工，所以自己的 codex 一個都認不出來）。
  正解是讀 `/proc/<pid>/cmdline` 裡的 `-C` 參數比對 session id。
  辨識特徵：**我的是 `codex exec -C ...`，他的是互動式 `codex --model gpt-5.6-sol ...`**。
  腳本在 `scratchpad/watchdog-v3.sh`（**寫成檔案不要內嵌 Monitor**，內嵌就改不動）。
- ⚠ **`uptime` 的 load average 判斷不了核心忙不忙**：codex 大半時間在等 API。
  2026-08-22 load 僅 0.12，但機器上同時有他 5 個 + 我 3 個 codex。**看行程清單，不要看 load。**
- ⚠ **模式比對會打到「自己」——昨晚踩三次**：`pgrep -af "codex exec"` 匹配到
  monitor 腳本自己（腳本文字含該字串）→ 永遠不判定結束；
  `for p in $(pgrep -f "wt-m5-9"); do kill $p` 匹配到**發出指令的 bash 自己** → 自殺。
  **一律 `pgrep -x <exact>` + 讀 `/proc/<pid>/cmdline`，不要 `pgrep -f` 加字串。**
- ⚠ **寄信建議要防抖 + 排除使用者自己的程式**。單次取樣會被 `7z` 解壓那種短命爆量誤觸；
  使用者打遊戲（`main` 537%）也會湊滿次數。**建議只是建議，先查證歸屬再決定寄不寄。**
  順帶：這種爆量正是「效能斷言取 N≥5 次最小值」那條裁定的活證據——
  單次牆鐘取樣若剛好落在這個窗口，`<10 ms` 的預算會假失敗。
- 📬 **與 Skyrim agent 的協定**：收件匣 `~/repo/moddings/skyrim/inbox/`。
  **CPU 我 35%（6 核）／他 45%；GPU 與桌面我全部讓出**，所以他開 Skyrim 不必先問我。
  監控權在我，超標我寄信、他不爭論；反之亦然。Monitor 每 30 秒掃兩邊收件匣。
- ⚠ **貼著 8 KB 上限**：`design/README.md` 因索引成長而超標，已依 AGENTS.md 慣例拆出
  `design/INDEX.md`（README = 入口導引，INDEX = 完整清單）。原始碼最大的是
  `tests/rules/ruleset_error_test.cpp` 7,921。**要動貼邊的檔就先拆**。


## ⚠ 合併衝突：純加法可以機械解，被切開的括號不行

三路並行都往 `core/rules/ruleset.{h,cpp}` 的同一處追加時，衝突區會**把函式主體切開**——
`{` 在衝突塊內、`}` 在塊外。這時用「兩邊都取」的通則批次解會**漏掉閉括號**：

```
...breakthroughs() const noexcept {
    return breakthroughs_;
...damage_types() const noexcept {     ← 少一個 }
    return damage_types_;
}
```

⚠ **編譯器的報錯離病灶很遠**：實際看到的是
`aetheria::rules::aetheria::world::RegionTiles has no member named 'edges'`
（命名空間套疊），完全指不到 `ruleset.h`。

- **宣告區**（`.h` 的成員與方法宣告）：兩邊直接串接，安全。
- **定義區**（`.cpp` 的函式本體）：兩邊之間要補 `}`，不能只串接。
- 預防：派工 prompt 明令**追加一律加在檔案最尾端的對應區塊**，不要插進中間既有的宣告群。

## 與 codex 的即時通道（2026-08-22 建立，實測有效）

規約見 [CODEX-PROTOCOL.md](CODEX-PROTOCOL.md)，腳本 `scripts/codex-watch.sh`。

**為什麼要有它**：回報只在 codex 做完並 commit 後才看得到。
中途卡住、發現任務書寫錯、需要裁定——這些**在舊做法裡完全沒有出口**，
codex 只能自己猜著做完，我才在回報裡看到「我假設了 X」。
而且 Monitor 綁在 session 上，session 一斷就瞎了（實測漏接過兩次完成）。

**關鍵設計：`.ask` 不會讓它停。** 規劃者不一定在線上，等於死鎖。
規則是「寫下問題 + 你採用的假設，然後照那個假設做完」。只有 `.blocked` 才停。

**實測接到的四個真問題**（都是舊做法裡看不到的）：
版本號碰撞、Site 損失該不該新增歸約列、vcpkg 的 Lua 已升 5.5、
free-while-emitting 的 Godot 警告。

⚠ **每個 worktree 各有自己的 repo 根**，codex 用相對路徑寫就落在它那邊——
watcher 已改成把每個 worktree 的 `.codex-inbox/` 都掃進來，不要求 codex 寫絕對路徑
（少一個會寫錯的地方）。

## ⚠ 兩個派工旗標的坑（2026-08-23 各踩一次）

- **不要帶 `-s workspace-write`**。使用者 config 是 `sandbox_mode = "danger-full-access"`，
  帶了就覆蓋它，codex 把 `.git` 當唯讀，`git merge` 在建 `.git/ORIG_HEAD.lock` 時
  exit 128 `Read-only file system`，一次 commit 都做不了。要嘛不帶，要嘛明寫
  `-s danger-full-access`。
- **`resume` 不吃自己後面的旗標**：`codex exec resume --last -C dir -s ...` 會把它們當
  prompt 而報 usage 錯。正確位置是 `exec` 之後、`resume` 之前：
  `codex exec -C <dir> -s danger-full-access -c ... resume --last "<prompt>"`。

## 📐 驗收的心得另立一檔

**[OPS-VERIFY.md](OPS-VERIFY.md)**：自己重跑負向控制為什麼不能省、
「會動的數字 vs 裝飾的數字」、門檻要從組成推導、`.ask` 何時值得打斷、
以及裁定過的兩條通則（導出資料不進存檔／存在 ≠ 同義）。
本檔只留**派工、限流、通訊**。

## ⚠ 派工任務書裡最值得寫的一句

> **如果你發現 X 其實可以被 Y 取代／誤差變大／某條規則一次都沒命中，
> 如實回報，不要硬湊一個情境糊過去。這比做完更有價值。**

實測有效：M7.2 自己回報「該固定戰役沒有碰巧創造騎兵側背條件，**沒有把它假報成覆蓋**」；
M6.7 用 `.ask` 講明「不冒充尚未存在的戰技模擬」；
M6.0 棄用了自己第一次太寬的注入，理由是「控制不夠單一」。
