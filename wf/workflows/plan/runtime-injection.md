# M10 可編輯的沙盒 — 動工計畫總覽

← [plan/README](README.md)｜調查佐證：[status](../investigation/runtime-injection-status.md)、
[assets](../investigation/runtime-injection-assets.md)｜裁定：[runtime-injection.md](../../../design/rules/runtime-injection.md)

**日期**：2026-08-27。目標＝使用者 2026-08-26 的裁定：**可編輯的沙盒（矮人要塞模型）**——
進行中的存檔可加入地形／文化／勢力／人物／物品定義；世界一個存檔、人物一個存檔。

## 一句話方案

> **世界存檔自帶它自己的 raws（TOML 原文複製進存檔），Ruleset 改成「只可在回合尾端追加」，
> 每筆注入是被記錄、可重放、可雜湊的歷史事件；玩家態拆成獨立的角色檔。**

四個支柱各自的關鍵選擇：

1. **raws 用 TOML 原文進存檔，不做 def 的二進位序列化。**
   開新世界＝把 `data/`（22 個 TOML）複製進 `saves/<世界>/raws/`，**之後永不改寫**
   （[裁定#8](runtime-injection-decisions.md)）；載入世界＝從它的 raws 載 Ruleset
   ＋重放日誌。量過的理由：18 類 def + 16 個單例規則塊、**9 處整數下標交叉引用**、
   1 處隱性陣列對齊（`TerrainGroundMapping` 靠位置對齊 `terrains_`，`ruleset.cpp:40-42`）——
   全部由既有載入器 fail-fast 解決。附帶收益：「改 `data/` 不會弄壞舊存檔」（DF 行為）
   與「def 內容變更有感知」（KNOWN-TRAPS 那條洞）同時免費解掉。四類字串 id 表照留當保險。
2. **Ruleset 從「不可變」改成「只可追加、只在回合尾端」。**
   下標＝載入順序，追加不位移；不可刪、不可改既有 def。實作形狀：`RulesetPatcher`
   friend，批次先在**影子 Ruleset** 整批重建驗證、成功才對現役追加（不得半套用；
   Ruleset 不可複製 `ruleset.h:38-41`，影子＝從 raws＋日誌＋本批重建一份）。
   ⚠ 追加**會**讓裸指標與 span 失效——`PowerSourceState::definition`／
   `RootDeityState::definition` 存的是 `const Def*`（`power_sources.h:49,199`），
   前置輪要改成存 id 查詢，並立規矩「span 不得跨回合持有」。
3. **歷史日誌記的是全部輸入：玩家命令＋注入。** 只記注入無法從 seed 重建世界——
   玩家移動、戰鬥、建造也改世界。重放＝seed＋基底 raws＋完整日誌；
   崩潰恢復走 journal-first（先落日誌＋commit marker，再套用；啟動時補放未套用尾巴）。
   結算掛在回合尾端的**新結算協調器**上（`queue_*` 有 `in_tick_` 防衛
   `zone_manager.cpp:189-201`，不能直接從 TurnEnd 呼叫——通道要自己接，不是現成的）。
   **世界身分雜湊**明定為 zone 正規化雜湊、raws 雜湊、日誌鏈頭三分量的**定序
   FNV-1a 鏈合成**（⚠ 不可用 XOR——XOR 允許分量互相抵消；2026-08-27 波 2 實測
   codex 就提出過「拿 raws_hash 種 genesis 讓兩項抵消」的湊數形狀）；
   空日誌的鏈頭＝固定常數 0，**永不從世界資料播種**；驗證工具三分量分開印，各自可稽核。
   M9 判準改寫成「**同 seed＋同日誌**重放兩次相同」。
4. **角色檔很小。** 量過玩家態只有 **6 個成員**（`player_army_id_`、`residence_`、`local_z_`、
   `local_player_x/y_`、`accepted_quest_id_`，`playable_session.h:245-260`），
   一律以字串 id／StableId 引用世界檔，自己不存型別。

## 輪次地圖

| 波 | 輪 | 變因 | 依賴 |
|---|---|---|---|
| 0 | M10.0a 原則五守門測試 | 內容種類 enum 的 ctest 白名單檢查 | — |
| 0 | M10.0b 雜湊改關係斷言 | 21 檔 14 常數的 golden snapshot 退場 | — |
| 0 | M10.0c 遊戲會存讀檔 | root zone + FileZoneStore + bridge/godot 入口 | — |
| 1 | M10.1 世界檔自帶 raws | Ruleset 載入來源改為存檔內 raws | 0c |
| 1 | M10.2 角色檔 | 玩家態 6 成員的獨立存檔 | 0c |
| 2 | M10.3a 歷史日誌＋結算協調器 | 命令/注入全記錄、重放、崩潰恢復、uid 配發 | 1 |
| 2 | M10.3b def 注入 | `RulesetPatcher`＋影子驗證＋指標 id 化；**通道通用於全部 def 型別**（[裁定#9](runtime-injection-decisions.md)） | 3a |
| 2 | M10.4 勢力可增長 | `add_faction` + codec 換字串 id 表；首個端到端注入（開拓）| 3b |
| 3 | M10.5a/b/c 原則五補課 | 兵種／建築／AI 目標動作資料化 | 0a |
| 4 | M10.6a/b/c 新內容型別 | 人物實體／文明文化 def／物品 | 3 |
| 5 | M10.7 注入入口 | 外部檔案吸收（`saves/<世界>/inject/`，裁定#2 已定） | 3b |
| 5 | M10.8 端到端整合驗收 | 全鏈驗收，見下方完成判準 | 全部 |

細目：波 0–1 → [runtime-injection-waves.md](runtime-injection-waves.md)；
波 2 → [runtime-injection-inject.md](runtime-injection-inject.md)；
波 3–5 → [runtime-injection-content.md](runtime-injection-content.md)；
並行線與領地 → [runtime-injection-dispatch.md](runtime-injection-dispatch.md)。

## 全局約束

- **原則九「拆得走」**（使用者 2026-08-27 裁定，[principles.md](../../../design/principles.md)）：
  第一個原型之後可能打掉重做，M10 的每個新能力都要是**可整組拆走的窄介面組件**——
  存讀檔編排、歷史日誌、`RulesetPatcher`、角色檔 codec、注入佇列各自成模組，
  **一個都不准長進 `PlayableSession`**（它已經是 god object，只准變薄不准變厚）。
  每份任務書都要有「新程式碼放哪個模組、依賴誰」一節，驗收時查依賴方向。
- **版本號只有整合輪動**，每波至多 +1（歷史撞過兩次，鐵律；軌跡見 dispatch）。
- **M10.0b 必須先於一切格式變更落地**，否則每次 bump 都要重寫 14 個常數。
- **不碰擱置項**：mark／unique-objects、root 成長軸（只量測不實作，見裁定#5）。
- 設計文件修訂隨計畫走：[definitions.md](../../../design/rules/definitions.md) 的
  「def 不進存檔／Ruleset 不可變」兩句按裁定#6 修訂——**由規劃者改，不派給 codex**。
- 實作全部派 codex gpt-sol；規劃者只做派工、驗收（重跑負向控制）、裁定。

## 裁定點

→ [runtime-injection-decisions.md](runtime-injection-decisions.md)。
**原本擋路的兩條已由使用者 2026-08-27 裁定**：注入來源＝外部檔案（#2）、
新勢力出現方式＝開拓先做、分裂與成長排人物實體之後（#3）；
SQLite 判定不換但留路（#7）、基底 raws 不可變（#8）。**沒有擋路的裁定點了。**

> 本計畫經 gpt-sol 唯讀審稿一輪（2026-08-27），5 組 27 條發現已全數吸收或覆核駁回；
> 修訂重點：日誌記全部輸入、基底 raws 不可變、影子驗證、結算協調器要自己接、
> 裸指標 id 化、faction remap 涵蓋 region owner、版本軌跡補 v25。

## 完成判準（M10.8 的驗收，整鏈一次過）

1. 開新世界（存檔自帶 raws）→ 玩若干旬 → 存檔關遊戲 → 重開讀檔，世界雜湊一致。
2. 進行中注入：**全型別矩陣**（每一類 def 至少注入一筆，[裁定#9](runtime-injection-decisions.md)
   ——「所有東西」）＋一個勢力（含 def＋外交＋領土）、一個具名人物、一件物品——
   各記入歷史日誌，回合尾端生效。
3. 注入後存檔→讀檔→**重放**（seed＋日誌重建）三者世界雜湊一致。
4. 改動全域 `data/` 之後，舊存檔照常開（讀的是自己的 raws）。
5. 同一世界開第二個角色檔，兩角色互不干擾。
6. 原則五 ctest 全綠（新增內容種類 enum 會紅）。
