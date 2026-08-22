# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](../AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log（若有「已落地功能目錄」則濃縮一句進去）。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾，按工作流／類別**拆檔 + 一個 index 導航**（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

本檔同時 ① 連到各工作流自己的 session-log（若該工作流已長出自己的），② 收**不屬任何工作流**的進度。

> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**（設計決策/修了什麼落到該工作流的文件、待使用者驗的進 [WAIT_USER](WAIT_USER.md)）。完成即整條刪除。

## 最新進度

> **分區塊擁有**：`### 規劃者` 與 `### 實作者` **各自只寫自己那塊，永遠不碰對方那塊**——
> 連順手整理都不行。跨區的事寫信。規約見
> [workflows/inbox/CONTACTS.md](workflows/inbox/CONTACTS.md) 的「同步規約」。
> **in-flight 一定要有記錄**：session 隨時會斷，本檔是唯一交接面。

### 實作者（gpt-sol）

- **待任務書**。

### 規劃者（Opus 5）

> **完成的不留這裡。** M0～M2.3 的結論都在 git log 與 `design/` 裡；
> 下面只留**還沒完成、或會在未來咬人**的東西。

## 📦 地形生成：已交接，等專門的 session 接手

使用者裁定**地形之後單開一個 session 處理**，本 session 到 M5.11 為止只留接口、不再調值。

**接手就從這裡開始**（一行指令重出全部診斷）：

```sh
./build/aetheria_sim gen terrain-metrics --seed 515151
```

**根因已經量到了，不必重新診斷**——濕度壓在 `uint16` 天花板上：

```
moisture_histogram  min=8833 median=61449 p95=65535 max=65535
  counts=28,67,110,73,104,124,86,94,139,136,137,149,179,213,238,1809
                                                                 ↑ 最後一格佔 49%
moisture_saturated_ratio=0.418      terrain_histogram swamp=1823（saturated=1541）
```

**swamp ≈ 飽和格**。因果鏈是：濕度削頂 → swamp 規則在所有飽和格上勝出 → 吃掉半張圖。
所以 M5.10 只能把 `moisture_scale` 收到 8000、壓到 49.457%，離 guard 只剩 **0.54% 餘裕**。

> ⚠ **這是同族缺陷第四次**。前三次是**門檻落在分布之外**（M1 沙漠、M5.4 的 tundra 與 swamp），
> 這次是**分布本身頂在天花板上**。表現不同，結果一樣：**分類規則失去鑑別力**。

**接縫**（`core/worldgen/field_redistribution.h`，兩個正式 overload 都是 identity）：
高度在 `erode_height` 後、`quantize_elevation` 前；濕度在河流回灌後、biome 分類前。
⚠ 未來放 rank remap **要用整數 LUT，不要 `pow`**（跨平台浮點不決定性）。

### ⚠ M5.18 找到了出貨遊戲的現成解：Freeciv 做了**兩件**我們沒做的事

1. **整數 CDF／rank remap**（`freeciv/server/generator/mapgen_utils.c:114-172` 的
   `adjust_int_map_filtered`）——統計每個整數值的頻率、算累積頻率、查表回寫。
   **高度與溫度都在離散化「之前」先等化**（`height_map.c:255-260`、`temperature_map.c:150-170`）。
   這是本輪找到**唯一**能直接回答「有沒有直方圖等化」的出貨遊戲證據。
2. **類別配額**（`mapgen.c:470-551`）——先算各 biome 的**目標格數**再逐類扣額度，
   候選不足就把剩餘額轉給 alternate。
   ⚠ **這比計分競爭更硬**：計分競爭仍允許某個 biome 到處贏，
   **配額讓「吃掉半張圖」在結構上不可能發生**。

⚠ **libtcod 的 `normalize` 只是 min/max 線性搬移，沒有 rank remap**——
它**拆不開已經被夾成 65535 的同值群**，對我們的削頂完全無效。

**不要**為了這個把風掃描退回獨立濕度 noise（M5.18 的結論）。

**排序建議**在 [m5-9-redblobgames-study-complete.md](inbox/m5-9-redblobgames-study-complete.md)
與 [m5-18-terrain-references-complete.md](inbox/m5-18-terrain-references-complete.md)；
⚠ 其中一條裁定：**coast distance 在 L1 相容，但各 Site／Local 自行 flood-fill 會覆寫邊界、
與降維裁決鏈衝突**——下放必須由 parent 全域先算再經 `BoundaryProfile` 傳。

**會在未來咬人**

- ⚠ **M4 的驗收不能用 byte 相等**：EnTT snapshot 只對同一段建構歷史決定性。
  世界級正規化雜湊已在 M2.0 落地（`aetheria_sim verify world-hash`）。
- ⚠ **`load` ≠ `rematerialize`**：`load` 只解碼持久層，程序層是空的。
  拿它當重新展開，往返測試**照樣通過**——寫進 [interface-lifecycle.md](../design/interface-lifecycle.md)。
- ⚠ **假通過的三個陷阱**（沒冷載入／值停在預設／只比頭尾），加上最隱蔽的
  **空的層必然通過**。M2.3 三條都踩過邊。寫進 [interface-verification.md](../design/interface-verification.md)。
- ⚠ **混淆點**：「戰鬥位階」與「聚合提升重要性」共用 significance 等級表但升級規則不同。
  見 `power-tiers.md` 末段。

**操作心得**

- 🔧 **派 codex**：`-c model_reasoning_effort="high"` 覆寫 config 的 `ultra`（60 分 → 12 分，品質沒掉）；
  prompt 要明令**不准 fan-out 自我審查子 agent**、**`--parallel 2`**（不帶數字會開滿核心）。
  並行用 `git worktree`，但要挑**檔案真正互斥**的兩件事。
- 🔧 **限流的歸屬判斷走「路徑」，不走名稱、也不走行程樹**（前兩種昨天都試過都錯）。
  按名稱會打到別的 agent 的同名實例（`cc1plus`／`ld`／`housecarl-mcp`，
  Skyrim agent 實測同時有 **10 份** houseCARL）。
  ⚠ **按行程樹也錯**：`nohup ... &` 派的 codex 會被 reparent 到 systemd，
  往上找 claude 那條鏈**會斷**——後果是**自己的 build 從來沒被限流**，
  而且它會建議「寄信指控對方」**而那些負載是自己的**。
  正解：`cwd` 落在本 session 的 scratchpad 底下才算自己的。腳本在
  `scratchpad/watchdog-v3.sh`（**寫成檔案不要內嵌 Monitor**，內嵌就改不動）。
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

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何工作流的進度

- （無）
