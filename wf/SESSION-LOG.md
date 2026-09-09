# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](../AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log（若有「已落地功能目錄」則濃縮一句進去）。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾，按工作流／類別**拆檔 + 一個 index 導航**（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

本檔同時 ① 連到各工作流自己的 session-log（若該工作流已長出自己的），② 收**不屬任何工作流**的進度。

> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**（設計決策/修了什麼落到該工作流的文件、待使用者驗的進 [WAIT_USER](WAIT_USER.md)）。完成即整條刪除。

## 最新進度

> **2026-09-09 使用者宣布專案解除凍結，先做完整世界規劃，不開始實作**。
> 目前方向與文件入口：[三層奇幻活世界 roadmap](workflows/plan/living-world/roadmap-2026-09.md)。舊實作停在 M10.3a。
> 讀 [波 2 交接](workflows/plan/runtime-injection-wave2-handoff.md)；
> 建置環境的 configure 配方見 [OPS-NOTES](OPS-NOTES.md)。

> **分區塊擁有**：`### 規劃者` 與 `### 實作者` **各自只寫自己那塊，永遠不碰對方那塊**——
> 連順手整理都不行。跨區的事寫信。規約見
> [workflows/inbox/CONTACTS.md](workflows/inbox/CONTACTS.md) 的「同步規約」。
> **in-flight 一定要有記錄**：session 隨時會斷，本檔是唯一交接面。

### 實作者（gpt-sol）

- 活世界待[裁定](../design/spec/spec-decisions.md)及實作指示；[交接](../design/spec/spec-review-next.md)。

### 規劃者（Opus 5）

> **完成的不留這裡。** M0～M2.3 的結論都在 git log 與 `design/` 裡；
> 下面只留**還沒完成、或會在未來咬人**的東西。

## 📌 進度：M8 完成（414/414），M9 呈現與內容開跑

**M0～M8 完成，目前 414/414。⚠ 遊戲已經能玩（見下）。**

M9 輪次規劃（**M8.3 先於 M9.0**，因為主線推進條件掛在城建上）：

| 輪 | 內容 | 存檔版本 | 並行 |
|---|---|---|---|
| M8.3 | 建築型別映射（住宅 ≠ 領主廳，拆掉 M8.2 的權宜 marker） | **獨佔 21** | 可與 M9.2 併行 |
| M9.2 | `tools/art/` 確定性後處理 + 入庫閘（Python，不碰 cmake） | 不動 | 檔案完全互斥 |
| M9.0 | 主線劇情跑完一條 + 通關 | **獨佔 22** | 接 M8.3 |
| M9.1 | 三層音景 | 不動 | 接 M9.0（都動 `godot/`） |

⚠ **M9 的兩條判準都不是「做完了」**：關掉 core 重開畫面與音景要完全一致；
同一存檔跑兩次通關、每個檢查點雜湊相同。

⚠ **待使用者決定**：M6.6 判定湧現任務「只有半步意義」，診斷是缺 **observer mark**、
歷史因果、具名後果。但 mark／獨特物件是使用者裁定**擱置**的，`milestones.md`
寫著「要做之前先問」。**沒問到之前不排這輪。**

**M6～M8.2 各輪全數完成**（力量體系→Lua 沙箱→可玩迴圈→城建/地城/湧現任務入迴圈；細節看 git log）。

⚠ **校準順序不可顛倒**：先把 Region 公式調到「打起來像那麼回事」，
再讓 Site 與 Local 去追它。反過來做會失控——低層自由度太高，拿它當基準等於沒有基準。

⚠ **「像不像真的」的第一次正面答案**（M6.6 實作者的主觀判斷，我要求他評的）：
湧現任務「**比殺 10 隻狼好，但只有半步意義，不算有靈魂**」——
尋人最像故事、運糧次之、**探索最像背景工作**。
他的診斷：缺 observer mark、歷史因果、完成後的具名後果接進模板。
**這條卡在上面那個「待使用者決定」上。**

## ✅ M5 下層完成（272/272）

**獨立一檔** → [M5-CLOSEOUT.md](M5-CLOSEOUT.md)。
抽象檢驗的結果，以及**「能動但不像真的」**那條未解問題都在那裡。

## 📦 地形生成：已交接

**獨立一檔** → [TERRAIN-HANDOFF.md](TERRAIN-HANDOFF.md)。
根因（濕度削頂）、量測入口、接縫位置、Freeciv 的 CDF + 配額，都在那裡。

## 🔴 2026-08-25：使用者的原始需求九個里程碑都沒被做

**「存檔進行中隨時加入任意文明」——設計文件從頭到尾沒寫過。**
它被翻譯成 `rules-extensibility.md` 的「熱重載」，而熱重載保證的是**不動世界狀態**，
需求要的正是動世界狀態。→ **[runtime-injection.md](../design/runtime-injection.md)**

**2026-08-27 調查完現況**（六項全沒實現，且**遊戲根本不會存檔**）→
**[investigation/runtime-injection-status.md](workflows/investigation/runtime-injection-status.md)**
（結論與差距）＋ [runtime-injection-assets.md](workflows/investigation/runtime-injection-assets.md)
（已有的地基與插入點）。

**2026-08-27 規劃完 M10 動工計畫**（新開 plan 工作流）→
**[plan/runtime-injection.md](workflows/plan/runtime-injection.md)**（總覽：一句話方案、
輪次地圖 M10.0a～M10.8、完成判準）＋ waves／content（波次細目）＋
decisions（裁定點）＋ dispatch（並行線與領地、版本號政策）。
計畫經 **gpt-sol 唯讀審稿**一輪、27 條發現全數吸收（日誌記全部輸入、基底 raws 不可變、
影子驗證、結算協調器要自己接、裸指標 id 化、faction remap 涵蓋 owner、v25）。
使用者當日追加四裁定：**原則九「拆得走」**（principles.md）、注入來源＝外部檔案、
勢力 origin（分裂／成長／開拓）是資料且開拓先做、**注入覆蓋面＝「所有東西」**
（裁定#9：通道通用於全部 def 型別＋全型別矩陣驗收，唯 Lua 劇情注入列 M11）；
SQLite 判定不換但留路（裁定#7）。
**沒有擋路的裁定點了。**

**2026-08-27 M10 波 0＋波 1 已併入 main**（詳情看 git log 與 inbox/done/）：v23、423 綠。
存讀檔＋原則五守門＋關係斷言（波 0）；世界檔自帶不可變 raws＋角色檔 v1（波 1）——
「世界一檔、人物一檔」已成立，「加勢力弄壞存檔」的 KNOWN-TRAPS 已死。
另完成三份呈現層調查。團隊改制 [TEAM.md](TEAM.md)：量大工作全給 codex，Claude 只留裁定與驗收。

🔶 **open：波 2 暫停於 M10.3a**（已併入 main、**仍 v23 未 bump**、432 綠；負向控制由波管家
親手跑）。待續 **M10.3b → M10.4 → v24 定版輪** →
**[波 2 交接](workflows/plan/runtime-injection-wave2-handoff.md)**
（v24 格式清單／繼承缺陷／多角色槽重放對不上）。

**會在未來咬人** → **獨立一檔** [KNOWN-TRAPS.md](KNOWN-TRAPS.md)
（假通過與驗證、原則五被侵蝕、AI 在地圖上什麼都不做、Region 沒有建築組成輸入）。

**操作心得** → **獨立一檔** [OPS-NOTES.md](OPS-NOTES.md)
（派 codex 的參數、限流歸屬怎麼判、即時通道）；
**驗收**心得 → [OPS-VERIFY.md](OPS-VERIFY.md)。

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何工作流的進度

- （無）

## 🎮 遊戲現在能玩到什麼（M8.1，2026-08-22）

`godot-mono --path godot` → 開新遊戲 → 選部隊 → 下移動意圖 → 推兩旬（AI 勢力真的會動）
→ 遭遇敵軍 → **選「親自指揮（Site）」或「讓系統算（Region）」** → 戰報 → 世界改變。

實測：`owner 2→1、人口 100→34、治安 20→0`；批次拉圖 **122880 bytes 一次 bridge 呼叫**
（不是逐格）。截圖在 `godot/artifacts/`。

⚠ **地形在真的遊戲畫面裡仍然很醜**——那是已交接的項目，見 [TERRAIN-HANDOFF.md](TERRAIN-HANDOFF.md)。
現在有真畫面可看，那個 session 的判斷依據比之前好。
