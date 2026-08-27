# M10 波次細目（波 2）：歷史日誌與注入通道

← [runtime-injection.md](runtime-injection.md)（總覽）｜波 0–1 → [runtime-injection-waves.md](runtime-injection-waves.md)｜波 3–5 → [runtime-injection-content.md](runtime-injection-content.md)

單線串行：M10.3a → M10.3b → M10.4。

## M10.3a 歷史日誌＋結算協調器

- **變因**：世界檔多一個 append-only 歷史日誌；回合尾端有了正式的結算協調器。
- 範圍：
  1. `saves/<世界>/history.log`：每筆＝`{tick, 種類, TOML payload, 前雜湊鏈}`，只追加。
     **記全部輸入**：玩家命令（issue_move／coverage_command／resolve_encounter 選擇／
     advance_xun）＋注入——只記注入無法從 seed 重建世界（玩家移動與戰鬥直接改
     registry 與 owner，`region_turn.cpp:118-160`、`playable_session.cpp:915-995`）；
  2. **結算協調器**：world 級的 TurnEnd commit point。⚠ 不是把 `queue_*` 直接掛上
     `TurnStage::TurnEnd`——那組 FIFO 有 `AETH_CHECK(in_tick_)` 防衛且只在
     `ZoneManager::tick()` 尾端 flush（`zone_manager.cpp:189-201,232`），與 Region 回合
     是兩條沒接合的生命週期。協調器負責：日誌先落盤（journal-first）→ commit marker →
     依序套用（def 先於實體）→ zone/manifest 落盤；
  3. **崩潰恢復**：一次注入橫跨 raws-hash／日誌／多個 zone `.bin`／manifest，
     單檔 tmp+rename 原子性（`save_manifest_io.cpp:37-57`）不夠——啟動時發現日誌
     有 commit marker 之後的未套用尾巴就重放它；
  4. **uid 統一走 manifest 配發器**（`next_detached_id`／`next_entity_uid` 已存在，
     `file_zone_store.h:28-36`）；session 裡寫死的 1001／2001／9001 退場——
     不統一配發，重載或重放後注入的實體必撞 uid；
  5. `sim` 加 `replay` 子命令：seed＋基底 raws＋日誌重建世界，比對雜湊；
  6. **世界身分雜湊**明定並實作：zone 正規化雜湊 ⊕ raws 雜湊 ⊕ 日誌鏈頭
     （現行 world-hash 只彙總 zone `.bin`，`sim/world_hash.cpp:59-73`——注入了 def
     但沒有實體引用它時雜湊不變，會誤判「歷史相同」）；
  7. 改寫 M9 判準與 [milestones.md](../../../design/milestones.md)：
     「同 seed＋**同日誌**重放兩次相同」（規劃者改文件，codex 不碰 design/）。
- ⚠ 原則九：日誌組件做成 **payload 無關**（append-only＋雜湊鏈＋重放介面，
  不 include world／site 型別）；世界專屬的 payload 解讀住在協調器。
- **Done when**：玩數旬（含移動、戰鬥、建造）→ `replay` 重建雜湊一致；
  日誌鏈載入時驗證，篡改中段 fail-fast；套用中途 kill 進程→重啟自動恢復到一致狀態；
  無任何注入時行為與 v22 全等。版本 bump → v23 前半（與 3b、R4 同波打包）。

## M10.3b def 注入（`RulesetPatcher`）

- **變因**：Ruleset 獲得「回合尾端追加」能力，**通道通用於全部 def 型別**
  （[裁定#9](runtime-injection-decisions.md)「所有東西」）；地形是驗收範例
  （它的最小包最刁鑽），不是唯一支援的型別。
- 範圍：
  1. **前置：裸指標 id 化**——`PowerSourceState::definition`／`RootDeityState::definition`
     存 `const Def*`（`power_sources.h:49,199`），vector 增長即懸空；改存 id、用時查。
     並審計「span 跨回合持有」，立規矩寫進 conventions；
  2. `RulesetPatcher` friend：只可追加，不可刪改；字串索引同步更新。
     **通用設計**：日誌 def 條目存「目標資料檔＋TOML 片段」，套用＝把片段餵給
     **既有載入器**的對應區段——不為任何型別寫專屬 patch 碼，載入器認得的就能注入；
  3. **影子驗證**：批次先從「基底 raws＋日誌＋本批」重建一份影子 Ruleset，
     整批過了才對現役追加（Ruleset 不可複製 `ruleset.h:38-41`，重建就是複製的替代；
     載入是毫秒級，成本可接受）——不得半套用，失敗＝整批拒收＋記失敗事件；
  4. **地形注入的最小包**＝`TerrainDef`＋`TerrainGroundMapping`（等長陣列缺格即
     載入失敗，`ruleset_load_site.cpp:75-119`；荒野生成查不到 mapping 直接 throw）
     ＋選配 `TerrainRule`（要參與 worldgen 才需要）——寫進注入 schema，缺件在影子驗證擋下。
- **Done when**：測試 API 於第 N 旬注入一種地形（完整最小包）→回合尾端生效→
  存／讀／重放三者雜湊一致；**全型別迴圈測試**：現有每一類 def 各注入一筆最小合法
  片段，全部通過同一條驗收（一類都不特判）；缺 mapping 的注入被整批拒收且世界不變；
  注入後 `PowerProfile` 等持有狀態照常運作（指標 id 化的負向控制）。

## M10.4 勢力可增長（首個端到端注入，origin＝開拓）

- **變因**：勢力數從建構期常數變成可在回合尾端 +1。
- 範圍：
  1. `WorldDiplomacyState::add_faction`：建構時配的是 relations／truths／knowledge／
     minds **四組**結構（`diplomacy.cpp:39-53`），矩陣 stride 從 `n+1` 變 `n+2`
     ——**逐列重排，不能直接 resize**；
  2. 外交 codec 的數量硬比對（`zone_diplomacy_codec.cpp:57,151`）換成**勢力字串 id 表
     ＋remap**；⚠ remap 範圍不只外交檔：Region 的 `tiles.owner` 直接序列化數字
     FactionId（`zone_encode.cpp:134-137`）、部隊 faction、外交 parties——
     **所有持久的 FactionId 引用一起納入**；
  3. session 的三處硬編碼一起退場：建構子 `3`（`playable_session.cpp:103`）、
     外交初始化只鋪 1–3（cpp:314-328）、AI 迴圈只迭代 {2,3}（cpp:880-889）——
     全改成「迭代現有勢力表」；
  4. 端到端：注入勢力 def＋`add_faction`＋**開拓**起始聚落（無主地，經協調器走
     `queue_materialize`；分裂／成長等人物實體之後，[裁定#3](runtime-injection-decisions.md)）；
  5. `data/faction_origins.toml`：origin 種類是資料（原則五），本輪只實作 `pioneer`。
- **Done when**：進行中的世界注入第 4 勢力→地圖上出現它的聚落、外交矩陣長一格、
  AI 開始為它出手；**同 v23 建立**的三勢力存檔照常開（不做跨版本 migration，
  fail-fast 政策不變）；重放一致。版本 bump → v23（整合輪打包 3a+3b+4）。
