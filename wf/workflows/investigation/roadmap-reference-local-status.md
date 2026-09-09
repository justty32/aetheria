# roadmap 參考盤點：小層 ToME4／RimWorld／Elin

← [investigation](README.md)｜2026-09-09｜唯讀外部參考

## 問題

小層主參考為 ToME4／RimWorld／Elin。中層其後已回歸原案；最新三層定位見 [藍圖](../plan/living-world/roadmap-three-layers.md)，不影響本檔小層研究。

本檔問：（1）三作各有哪些可靠的小層機制可借？（2）證據是原作核對、早期 analysis，還是
未實作構想？（3）小層應停在哪裡，不能反過來接管大層世界與中層戰略？

## 結論

**三作合起來適合回答小層的三面：ToME4 管逐格探索、技能、物品與任務內容；RimWorld 管人物
狀態、工作排程、資源競爭與局部場景濃縮；Elin 提供區域生成、巢狀行為與對話劇情的候選切法。**
ToME4 knowledge 與 RimWorld 反編譯核對可當較強證據；Elin 玩法總覽和 ToME4 Erebus 都只能當
線索／構想。三者都不能證明 aetheria 已有文明長時模擬或中層軍團戰役接口。

## 現況

### ToME4：探索與內容積木

**證據等級：以下三點來自已回原始碼複驗的 knowledge。**

1. **同一 zone 容器，不同玩法政策。**原版大地圖也是 `persistent="zone"` 的普通 zone，
   `wilderness=true` 才整包改時間、FOV、技能／道具互動。可借「共同容器＋可拆政策」，不借這個
   綁死的旗標包。[證據](</home/lorkhan/repo/moddings/tome4/docs/knowledge/worldmap-parts/01-basics.md:6>)
2. **探索有明確事件掛點。**對話、進 zone、首次生成、擊殺、踩地形可推任務；lore 可由拾取、
   死亡或隨機撒點取得，適合 Local／Site 把地形、人物與戰果變成發現。
   [證據](</home/lorkhan/repo/moddings/tome4/docs/knowledge/quests-and-lore.md:49>)
3. **技能與物品是資料積木。**技能有需求、模式、學習／使用／逐回合回呼；物品用基底、詞綴、
   觸發器、rarity／深度池與 unique 去重組合，也有擊殺成長、吸收技能的前例。
   [技能](</home/lorkhan/repo/moddings/tome4/docs/knowledge/class-parts/01-birth-and-talents.md:68>)、
   [物品](</home/lorkhan/repo/moddings/tome4/docs/knowledge/items-and-egos.md:39>)、
   [生成](</home/lorkhan/repo/moddings/tome4/docs/knowledge/items-and-egos.md:112>)

zone 各有 grid／NPC／object／trap 清單，每層可深合併生成設定，適合當小層內容邊界。
[證據](</home/lorkhan/repo/moddings/tome4/docs/knowledge/worldmap-parts/02-adding-to-eyal.md:77>)。對舊世界加地點採
append＋overlay、不覆寫整張圖，值得借「加法相容」原則，但 Lua hook 不是 aetheria API。
[證據](</home/lorkhan/repo/moddings/tome4/docs/knowledge/worldmap-parts/02-adding-to-eyal.md:6>)

**較弱證據：**早期 analysis 所稱 World→Zone→Level、三種持久策略與最近 zone LRU 只是索引，
repo 明訂不得當權威。[線索](</home/lorkhan/repo/moddings/tome4/docs/analysis/architecture/engine_detail/3-世界結構.md:1>)、
[警語](</home/lorkhan/repo/moddings/tome4/AGENTS.md:30>)。Erebus 的三層、地形×勢力×世界狀態生成、
LRU／seed 重建與「發現即固化」是**未實作且未複驗構想，不是原作能力**。
[構想](</home/lorkhan/repo/moddings/tome4/wf/workflows/specs/erebus/02-mechanics.md:74>)、
[生命週期](</home/lorkhan/repo/moddings/tome4/wf/workflows/specs/erebus/02-mechanics.md:133>)、
[警語](</home/lorkhan/repo/moddings/tome4/wf/workflows/specs/erebus/02-mechanics.md:5>)

### RimWorld：人物工作與局部場景

**證據等級：分析以 1.6／Odyssey 反編譯碼核對，已知錯誤會在行內警告。**
[範圍](</home/lorkhan/repo/moddings/rimworld/analysis/rimworld/README.md:47>)

1. **工作不是直接下動作。**ThinkTree 選 JobGiver，產生 Job，再由 JobDriver 拆成 Toil；
   Reservation 先占用物件或位置，避免兩人同搶資源。適合居民自主行為與可解釋排程。
   [證據](</home/lorkhan/repo/moddings/rimworld/analysis/rimworld/architecture/ai_deep_dive.md:5>)
2. **人物按 tracker 分責任。**健康、技能、需求、心智、工作各自保存與更新，不把人物做成一坨
   巨型狀態。[證據](</home/lorkhan/repo/moddings/rimworld/analysis/rimworld/architecture/entity_system.md:29>)
3. **短事件與長任務分軌。**Incident 是單次事件；Quest 用節點、Signals、Slate 保存跨步驟上下文。
   [證據](</home/lorkhan/repo/moddings/rimworld/analysis/rimworld/tutorial/incident_and_quest_system.md:3>)

生命週期上，Map 留在 `Game.Maps` 就必須 tick；拆圖時 Pawn 可轉 WorldPawn，地形建物則丟棄，
PocketMap 有官方生成／銷毀先例。因此非焦點或臨時場景可濃縮數值、需要時重建；**承諾永久基地的
zone 不能全套這種丟棄法。**[證據](</home/lorkhan/repo/moddings/rimworld/analysis/rimworld/architecture/outpost_archiving_strategy.md:14>)、
[限制](</home/lorkhan/repo/moddings/rimworld/analysis/rimworld/architecture/outpost_archiving_strategy.md:44>)

### Elin：生成、行為與劇情的候選切法

**證據等級：玩法總覽是早期 analysis；knowledge 只驗過 package／script，以下只能列候選方向。**
[警語](</home/lorkhan/repo/moddings/elin/docs/README.md:1>)

1. `World→Region→Zone→Map→Cell` 分尺度；ZoneBlueprint／Profile 把區域定義與生成策略拆開，
   地形生成再與 Populate 放入內容分段。[線索](</home/lorkhan/repo/moddings/elin/docs/analysis/architecture/04_level4_deep_systems.md:7>)
2. NPC 以 Goal 指向巢狀 AIAct，行為有 Running／Fail／Success 與重試；TaskPoint 表示可認領工作點。
   [線索](</home/lorkhan/repo/moddings/elin/docs/analysis/architecture/05_level5_ai_drama.md:79>)
3. Drama 把 sequence、actor、choice、event 分型，提示劇情內容與執行狀態可以分離。
   [線索](</home/lorkhan/repo/moddings/elin/docs/analysis/architecture/05_level5_ai_drama.md:163>)

Elin 的 Unity／C# 協程與繼承架構不能搬進純 C++ core；目前只能借上述責任切分。

## 差距

1. ToME4 是玩家角色中心 roguelike；第二張大地圖還會因全局僅一對 `wild_x/y` 而串位，不能充當
   aetheria 三層定址。[證據](</home/lorkhan/repo/moddings/tome4/docs/knowledge/worldmap-parts/02-adding-to-eyal.md:43>)
2. RimWorld 的濃縮／銷毀適用臨時場景，不等於永久聚落可無損卸載；人物保留也不等於社會仍在跑。
3. Elin 三項都待回原始碼或實測複驗，現階段不能寫進硬性驗收判準。
4. 三作都沒有直接證明大層文明演化或中層軍隊／城鎮戰役接口；小層不得越界補這兩層。
5. 三作的 Lua／C#／Unity API 與 C++ core＋Godot 呈現邊界不同，只能借領域模型。

## 牽扯到的部份

- 小層責任：Local／Site 的逐格探索、人物需求與工作、戰鬥技能、物品、短事件、長任務與 lore。
- zone 生命週期：非焦點濃縮、臨時場景重建、永久基地保存是三種不同承諾；若採用，會碰
  `ZoneKey`、LOD、存檔、釘選與跨層歸約，需另進 spec，本調查不先選方案。
- 舊世界加內容：只保留「增量、可共存」原則；aetheria 的人物／物品／定義注入現況仍以
  [runtime-injection-status.md](runtime-injection-status.md) 為準。
- 技能、物品、工作、任務若資料化，會牽動 C++ rules／序列化／事件與 Godot 呈現；外部作品有模型
  不代表 aetheria 已有相同擴充能力。
