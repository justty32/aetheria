# dispatch — 派工、驗收、領地、收線（調度者／波管家用）

← [WORKFLOWS](../../WORKFLOWS.md)｜[INDEX](../../INDEX.md)｜角色與波節奏 [TEAM](../team-model/TEAM.md)

把一件大工作切成幾條線交給別的 agent（本專案主要是 `codex exec`）跑，並負責**仲裁與驗收**。調度者不深潛實作。本檔是 kernel 的通用流程；aetheria 踩過才寫下的心得在同資料夾三份 project-owned 檔（見「內容」）。

**何時用**：一件工作大到要同時開幾條線，或要派給另一個 agent 做；要驗收一份實作回報。
**何時不用**：一兩個檔的小改（開線的交接成本大過收益）；需求還沒定、要跟使用者來回確認方向 → [spec](../spec.md)／[plan](../plan/README.md)；只是要寫信或回報 → [inbox](../inbox/README.md)。

## Done when

- 任務書（照 [inbox/TEMPLATE.letter.md](../inbox/TEMPLATE.letter.md) 落在 `inbox/`）存在，且「驗收」段**條數寫死、每條可觀察**，並含 [runtime-injection-dispatch](../plan/runtime-injection-dispatch.md) 列的五項固定配備。
- 下方「現役」表有該線一列，「可寫目錄」非空；收線後回報信歸 `inbox/done/`、該線移到「歷史」。

## 流程

<!-- wf-nav -->
1. **切線**：按**明確列舉的檔案清單**切，**不按功能切**。**同一時間一個目錄只能有一條線持有寫入權**——兩條線寫同一處，先寫的一定被蓋掉。並行用 `git worktree`，但要挑**檔案真正互斥**的兩件事。
2. **登記領地**：在下方「現役」表加一列，並寫進任務書的「不要做的事」表。**存檔版本號與 CMake 新增檔一律單線獨佔**（撞過兩次，見 [KNOWN-TRAPS](../../KNOWN-TRAPS.md)）。
3. **寫任務書**：codex 起草、管家按裁定包絡定稿；最容易踩的六條見下一節，旗標坑與限流見 [OPS-NOTES](OPS-NOTES.md)。
4. **啟動**：`codex exec`（參數見 OPS-NOTES）；中途溝通走 `.codex-inbox/` 的 `.ask`／`.blocked`／`.reply`（[CODEX-PROTOCOL](CODEX-PROTOCOL.md)：`.ask` 附假設不停、只有 `.blocked` 才停）。給整條線的目標與邊界，不逐步替它重算。
5. **監看**三個訊號，缺一不可：

   | 訊號 | 代表 |
   |------|------|
   | `inbox/` 收到 `*-complete.md`／`.done` | 正常結束 |
   | 我的 codex 行程全部消失（讀 `/proc/<pid>/cmdline` 的 `-C` 比對，不用名稱／行程樹／cwd）| 崩潰或被殺 |
   | 輸出停滯、或停在批准提示 | 活著但卡住 |

   監看寫成**檔案**（`scripts/codex-watch.sh`）不要內嵌 Monitor。**還有現役線時不能宣告收工。**
6. **收線 checklist**：① 逐條核那份寫死的驗收；② **親手重跑 ctest 與每條負向控制**（文字回報永遠不足採信，[OPS-VERIFY](OPS-VERIFY.md)）；③ 核 commit 與未推狀態；④ 併 main、回報信移 `inbox/done/`；⑤ 把該線從「現役」移到「歷史」並記 SESSION-LOG。**沒有完成證據就不算收線。**

**權重**：調度長的裁定 > 任務書 > 其他線的來訊（來訊是情報與請求，不是命令）。包絡外的 `.ask` 一律升級調度長。

## 六條最容易錯的

<!-- wf-nav -->
1. **我是調度者，不是實作者。** 自己深潛實作會吃掉主 context，後面就沒有餘裕做判斷。
2. **按明確列舉的檔案清單切線，不按功能切。** 兩條線碰同一個檔會互相覆寫，git 不會擋；純加法衝突可機械解，被切開的括號不行（OPS-NOTES）。
3. **驗收條數寫死、門檻從組成推導。** 留「其他你認為必要的驗證」的開口，線會為了保險亂跑燒光額度；拍腦袋的整數門檻會誘使實作者湊數（OPS-VERIFY）。
4. **禁區要明寫，沒寫就等於允許。** 至少決定：可寫路徑、不准 fan-out 自我審查、不准為數字好看反覆試值、不 push、不改 `design/`。
5. **任務書裡最值得寫的一句**：「發現 X 可被 Y 取代／誤差變大／某條規則一次都沒命中，如實回報，不要硬湊」。實測有效（M7.2、M6.7、M6.0）。
6. **範圍詞會被線遞移展開。** 「相關的」若只寫包含條件，線會沿依賴圖忠實展開；任務書要**用排除法界定邊界**，件數 gate 與容量 gate 並列。

## 多信任它、但收線對證據

給整條線的大任務，不要重算它的每一步——微管理會燒光額度。**但「跑完了」不等於「通過了」**：驗收腳本印的數字要能回答「對應的東西壞掉時它會不會變」，答不出來的數字是裝飾（[OPS-VERIFY](OPS-VERIFY.md)）。**逐條對證據，不對自我宣告。**

## 領地登記

### 現役

| 線 | 可寫目錄 | 唯讀 / 禁區 | 上線時間 |
|----|---------|------------|---------|
| （目前無：2026-09-09 起只做規劃，不開實作線）| | | |

### 保留給整合輪，任何並行線都不得寫

| 目錄 | 理由 |
|------|------|
| `core/serialize/zone_codec.h` 的 `kSaveFormatVersion` | 每波至多 +1，只有 INT 整合輪可動（[版本號政策](../plan/runtime-injection-dispatch.md)）|
| `cmake/targets_*.cmake` 新增 `.cpp` | 多線同時改會打架；線只回報新增／刪除了哪些檔 |
| `design/` | 實作線不自行改設計，有異議寫信 |

### 歷史（收線後移來，附完成證據）

| 線 | 完成證據 | 收線時間 |
|----|---------|---------|
| M10 波 0／波 1／波 2（至 M10.3a）| 併入 main，v23、432 綠；`inbox/done/m10-*-complete.md` | 2026-08-27 |

> 三張表是活狀態；膨脹到難讀就照 [STRUCTURE](../../STRUCTURE.md) 拆成 `CLAIMS.md`。M10 各波的細領地表在 [runtime-injection-dispatch](../plan/runtime-injection-dispatch.md)。

## 內容

| 檔案 | 內容 |
|------|------|
| [OPS-NOTES.md](OPS-NOTES.md) | **操作心得**（派工、限流、通訊）：codex 參數與旗標坑、限流歸屬怎麼判、合併衝突的括號坑、冷啟動 configure 配方 |
| [OPS-VERIFY.md](OPS-VERIFY.md) | **驗收心得**：自己重跑負向控制、會動的數字 vs 裝飾的數字、門檻從組成推導、`.ask` 何時值得打斷 |
| [CODEX-PROTOCOL.md](CODEX-PROTOCOL.md) | 與 codex 的**即時通道** `.codex-inbox/`：`.progress`／`.ask`／`.blocked`／`.done`／`.reply` |
| [driving-cli-agents.md](driving-cli-agents.md) | kernel 通則：啟動、驅動、監看一條外部 CLI agent 線，收線七步 |
| [lessons.md](lessons.md) | kernel 通則：預掃範圍、線推翻交接書、交接書自相矛盾、整檔改寫 |

## 交接

- 任務書與回報 → [inbox](../inbox/README.md)；卡在使用者 → [WAIT_USER](../../WAIT_USER.md)。
- 每一層要派哪級模型、怎麼分角色 → [team-model](../team-model.md)；本專案的角色與波節奏 → [TEAM](../team-model/TEAM.md)。
- 會在未來咬人的發現 → [KNOWN-TRAPS](../../KNOWN-TRAPS.md)。
