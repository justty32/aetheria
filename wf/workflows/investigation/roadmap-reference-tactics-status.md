# roadmap 參考盤點：VCMI／Wesnoth 與中層戰棋

← [investigation](README.md)｜2026-09-09｜唯讀參考：`~/repo/pas`

## 問題

調查時方向為中層參考 VCMI／Wesnoth；使用者其後確認回歸原案的回合制方陣戰場與地方經營。本檔保留兩作的研究事實，現作輔助參考；最新定位見 [三層藍圖](../plan/living-world/roadmap-three-layers.md)。

本檔只問：兩作的初始化、單位、行動、地形、AI、回寫可參考什麼？哪些只是原作政策？

## 結論

**兩作說明了中層責任與跨層接口，但沒有替 aetheria 裁定單位粒度、格子或回合政策。**

VCMI 可參考「大圖軍隊 → 暫態戰場 → 結果回寫」；Wesnoth 可參考地形、移動、攻防與 AI 共用
棋盤查詢。兩者支持 BattleContext／BattleResult 邊界，但速度佇列、整側行動，以及本案可能的
命令收齊後同時結算，是三種待對照政策，不能由調查代為裁定。

## 現況

### VCMI：獨立戰場是大地圖軍隊的暫態投影

- 開戰先決定戰場與地形、封鎖玩家、送出快照、建立戰鬥查詢，再跑開場觸發器與首回合。可借的是
  「封存輸入、建立戰場、執行開場規則、才接受命令」的階段界線
  （[L4-battle-turn.md](</home/lorkhan/repo/pas/analysis/vcmi/architecture/L4-battle-turn.md:44>)、
  [開場流程](</home/lorkhan/repo/pas/analysis/vcmi/architecture/L4-battle-turn.md:47>)）。
- 兵力分成靜態模板、大圖堆疊、戰鬥活體，活體指回來源。可借「穩定身分＋各層投影」，不等於
  必須採聚合兵數
  （[L3-creature.md](</home/lorkhan/repo/pas/analysis/vcmi/architecture/L3-creature.md:18>)、
  [戰場活體](</home/lorkhan/repo/pas/analysis/vcmi/architecture/L3-creature.md:64>)）。
- 原作七槽與寬／密隊形只是內容限制，不能抄成常數
  （[CCreatureSet](</home/lorkhan/repo/pas/analysis/vcmi/architecture/L3-creature.md:81>)）。
- 每回合動態排 stack：攻城器、正常、等待分階段，速度與攻守交替再定序，士氣可再動。可借排程器
  邊界，不借順序政策（[行動順序](</home/lorkhan/repo/pas/analysis/vcmi/architecture/L4-battle-turn.md:50>)、
  [行動後](</home/lorkhan/repo/pas/analysis/vcmi/architecture/L4-battle-turn.md:71>)）。
- BattleAI 複製假想戰場、快取傷害、列目標、模擬交換。可借「副本評估、真狀態只收命令」，
  不借 AIValue 或模擬回合數
  （[L5-battleai.md](</home/lorkhan/repo/pas/analysis/vcmi/architecture/L5-battleai.md:9>)、
  [決策流程](</home/lorkhan/repo/pas/analysis/vcmi/architecture/L5-battleai.md:54>)）。
- 終戰形成結果，再確認傷亡、更新來源軍隊、延後升級，最後清戰場、解除大圖封鎖
  （[戰鬥結算](</home/lorkhan/repo/pas/analysis/vcmi/architecture/L4-battle-turn.md:74>)）。

### Wesnoth：整張戰術棋盤共用同一套地形與規則查詢

- scenario 帶地形、資源、事件、目標與 side 起點。可借「戰場定義與參戰狀態一起輸入」，
  不代表必須一關一圖或使用 WML
  （[tech_encyclopedia_vol4_generators.md](</home/lorkhan/repo/pas/analysis/wesnoth/details/tech_encyclopedia_vol4_generators.md:12>)、
  [起點](</home/lorkhan/repo/pas/analysis/wesnoth/details/complete_manual_vol1_map.md:45>)）。
- 原作整側回合；`new_turn` 恢復該側移動與攻擊。它和速度佇列、命令同時結算都是待比對政策
  （[complete_manual_vol12_game_board.md](</home/lorkhan/repo/pas/analysis/wesnoth/details/complete_manual_vol12_game_board.md:11>)、
  [原碼 game_board.cpp](</home/lorkhan/repo/pas/projects/wesnoth-master/src/game_board.cpp:70>)）。
- A* 的成本由單位移動類型與地形決定；進入敵方控制區時，以剩餘移動力作成本，令單位當回合停步。
  可借的是「尋路與規則共用 cost query」，不代表一定採六角格或一定要 ZOC
  （[complete_manual_vol3_pathfinding.md](</home/lorkhan/repo/pas/analysis/wesnoth/details/complete_manual_vol3_pathfinding.md:34>)、
  [A* 原碼入口](</home/lorkhan/repo/pas/projects/wesnoth-master/src/pathfind/astarsearch.cpp:129>)）。
- 地形也影響命中，再受武器特效修改；可檢查尋路、AI、結算是否共用規則
  （[gameplay_mechanics.md](</home/lorkhan/repo/pas/analysis/wesnoth/details/gameplay_mechanics.md:3>)）。
- AI 把招募、進攻、移動拆成候選行動，攻擊評估含 HP 機率與站位曝險；RAII 試擺可自動復原，
  不污染真棋盤
  （[ai_decision_engine_models.md](</home/lorkhan/repo/pas/analysis/wesnoth/details/ai_decision_engine_models.md:7>)、
  [戰鬥評估](</home/lorkhan/repo/pas/analysis/wesnoth/details/ai_decision_engine_models.md:16>)、
  [temporary unit](</home/lorkhan/repo/pas/analysis/wesnoth/details/complete_manual_vol12_game_board.md:26>)）。
- 生還單位保留經驗與特質進召回名單，跨關另轉移金錢與招募資料。這是個體回寫例，不表示
  aetheria 必須把軍隊展開成永久單兵
  （[召回名單](</home/lorkhan/repo/pas/analysis/wesnoth/details/complete_manual_vol12_game_board.md:20>)、
  [原碼 carryover.cpp](</home/lorkhan/repo/pas/projects/wesnoth-master/src/carryover.cpp:46>)）。

## 差距

1. **時間政策未裁定。** 單位輪替、整側行動、命令同時結算，對反應、集火、ZOC、AI 後果不同。
2. **尚未裁定軍力投影粒度。** VCMI 是同兵種聚合 stack，Wesnoth 是持久個體；兩者都只證明
   「來源狀態與戰場投影要可追溯」，不能推出七槽、單兵或固定隊伍上限。
3. **格子政策未定。** 六角、ZOC、地形防禦只是範例，仍須對照三層座標與跨層縮放。
4. **兩套 AI 不能直接共用分數。** VCMI 評的是當前 stack 的局部傷害交換；Wesnoth 評的是整側
   候選行動與隨機 HP 分布。若沒有共同命令、查詢與結果格式，只會得到兩套彼此矛盾的決策器。
5. **原作結果都不是本案回寫規格。** VCMI 回寫聚合兵數，Wesnoth 保存個體；本案仍缺軍隊、
   人物、地點、勢力與時間如何接收 BattleResult 的裁定。

## 牽扯到的部份

- 中層需服務同一個三層世界：大層提供參戰軍隊、地點、地形與時間背景；中層只建立所需投影；
  結果再回寫同一批穩定身分。不能把中層另做成與大／小層斷開的獨立戰役遊戲。
- 後續 spec 需一起對照：命令收集與結算時機、軍隊／單位投影、戰場生成輸入、地形 cost／combat
  query、AI 可見資訊與模擬副本、BattleResult schema、撤退／俘虜／死亡／成長及存檔版本。
- VCMI 的七槽與速度序、Wesnoth 的六角格／單兵／整側回合，均只能列為參考政策；本報告沒有
  改動設計文件、roadmap、core、bridge 或 godot，也沒有執行兩作。
