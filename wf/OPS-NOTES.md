# OPS-NOTES — 操作心得（派工、限流、通訊）

← [SESSION-LOG.md](SESSION-LOG.md)｜[AGENTS.md](../AGENTS.md)

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

