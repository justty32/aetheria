# roadmap 參考盤點：人物、成長與事件如何跨三層

← [investigation](README.md)｜2026-09-09

## 問題

roadmap 採三層主骨架；中層其後已回歸原案城建與方陣戰場，最新分工見 [藍圖](../plan/living-world/roadmap-three-layers.md)。本檔研究跨層人物。
本報告判定三款修仙遊戲能補哪些跨層人物、成長與事件機制、哪些不能照搬、哪些尚未證實。

longyin／sanguo 由主調查另補。

## 結論

**三作只補人物與世界活性，不取代三層主骨架。**共同方向是人物保有跨層身分與歷史，
各層只建立所需投影，事件再回寫人物、勢力與地點；這不是本案既有實作或方案裁定。

## 現況

### 《鬼谷八荒》：人物先做事，故事後來才說

- NPC 先抽動機，再選目標與行動；玩家與 NPC 共用改變世界的行動規則。跨人物請求雙邊保存，
  可跨月續跑（[npc-ai.md](</home/lorkhan/repo/moddings/guigu/wf/workflows/investigations/npc-ai.md:17>)、
  [目標與行動](</home/lorkhan/repo/moddings/guigu/wf/workflows/investigations/npc-ai.md:23>)）。
- 穩定身分、單向感情、人情分開保存；殺人會讓親友記仇。日誌按性格、關係與記憶選文，
  舊事不因後來關係改變而改寫
  （[relations-events.md](</home/lorkhan/repo/moddings/guigu/wf/workflows/investigations/relations-events.md:25>)、
  [關係三層](</home/lorkhan/repo/moddings/guigu/wf/workflows/investigations/relations-events.md:35>)）。
- 月結固定為角色行動→世界結算→刷新地圖與事件，跨層結果有固定回寫次序
  （[month-tick.md](</home/lorkhan/repo/moddings/guigu/wf/workflows/investigations/month-tick.md:23>)）。

### 《覓長生》：一個人物，依所在層換重量

- NPC 平時是輕量可變紀錄，打架時才投影成 Avatar；可對應大層摘要、中層成員、小層戰鬥者
  （[data-structs.md](</home/lorkhan/repo/moddings/michangsheng/wf/workflows/investigations/data-structs.md:5>)、
  [設計啟示](</home/lorkhan/repo/moddings/michangsheng/wf/workflows/investigations/data-structs.md:89>)）。
- 玩家行為才推日期；NPC 以月結游標追趕，跨度大時用近似算法，固定劇情按絕對日期補齊
  （[time-tick.md](</home/lorkhan/repo/moddings/michangsheng/wf/workflows/investigations/time-tick.md:7>)、
  [效能](</home/lorkhan/repo/moddings/michangsheng/wf/workflows/investigations/time-tick.md:61>)）。
- 跨 NPC 行為本月登記、下月收尾，不要求所有人物同時在線
  （[time-tick.md](</home/lorkhan/repo/moddings/michangsheng/wf/workflows/investigations/time-tick.md:44>)）。

### 《了不起的修仙模擬器》：只替玩家看得到的地方付細算成本

- 玩家已開拓區域細算；遠方勢力只存摘要與計時器，人物只存種子，出場才生成；商店查看時補算
  （[time-progression.md](</home/lorkhan/repo/moddings/xiuxian/wf/workflows/investigations/time-progression.md:43>)）。
- 玩家與 NPC 宗門不是同重量物件，但共用 ID，提供「同身分、不同解析度」先例
  （[data-structs.md](</home/lorkhan/repo/moddings/xiuxian/wf/workflows/investigations/data-structs.md:55>)）。
- 事件有隨機最短、強制最長間隔，另疊固定日期編年史
  （[time-progression.md](</home/lorkhan/repo/moddings/xiuxian/wf/workflows/investigations/time-progression.md:51>)）。

## 差距

1. **這些不是 aetheria 現況。** 尚未確認本案已有跨層人物、投影、事件回寫或時間降階。
2. **《鬼谷八荒》不能照搬其規則層。** 條件指令缺型別檢查，行動失敗只留是／否；月結又把
   66 個入口塞在世界主結算，難以追因果
   （[npc-ai.md](</home/lorkhan/repo/moddings/guigu/wf/workflows/investigations/npc-ai.md:91>)、
   [month-tick.md](</home/lorkhan/repo/moddings/guigu/wf/workflows/investigations/month-tick.md:99>)）。
3. **《覓長生》沒有可借的勢力生命週期。** 宗門只是人物身上的 ID 與靜態表，不會成長、囤積、
   交戰；其反射存檔也沒有 schema 或版本遷移
   （[time-tick.md](</home/lorkhan/repo/moddings/michangsheng/wf/workflows/investigations/time-tick.md:54>)、
   [data-structs.md](</home/lorkhan/repo/moddings/michangsheng/wf/workflows/investigations/data-structs.md:57>)）。
4. **《修仙模擬器》的遠方抽象過頭。** NPC 宗門不自行成長，人物不隨時間變強，據點不會易主；
   只能借成本分層，不能借它的世界活性
   （[time-progression.md](</home/lorkhan/repo/moddings/xiuxian/wf/workflows/investigations/time-progression.md:43>)、
   [buildings-cities.md](</home/lorkhan/repo/moddings/xiuxian/wf/workflows/investigations/buildings-cities.md:39>)）。
5. **未證實處明列：**《鬼谷八荒》的地圖共用亂數是否由上游固定、宗門戰完整公式、長期 NPC
   行為分布未證實（[world-gen.md](</home/lorkhan/repo/moddings/guigu/wf/workflows/investigations/world-gen.md:100>)、
   [month-tick.md](</home/lorkhan/repo/moddings/guigu/wf/workflows/investigations/month-tick.md:115>)）；《覓長生》缺真實存檔、
   Unity 場景與 Prefab，節點鄰接和實際手感未驗證
   （[data-structs.md](</home/lorkhan/repo/moddings/michangsheng/wf/workflows/investigations/data-structs.md:97>)）；
   《修仙模擬器》的預製戰場資產、部分世界欄位與新局異常地點仍需實機核對
   （[buildings-cities.md](</home/lorkhan/repo/moddings/xiuxian/wf/workflows/investigations/buildings-cities.md:68>)、
   [world-gen.md](</home/lorkhan/repo/moddings/xiuxian/wf/workflows/investigations/world-gen.md:74>)）。

## 牽扯到的部份

- roadmap 判讀時，要把三作放在「跨層人物身分／成長摘要／關係記憶／事件回寫／時間降階」；
  地圖、戰鬥與探索主骨架以最新三層藍圖為準，不由這三作取代。
- 後續 spec 需核對人物／勢力模型、投影生命週期、時間、事件歷史與存檔版本；本報告不裁定方案。
- 舊世界追加文明、人物、物品會同時碰到執行期注入現況；目前人物仍只是極薄紀錄，物品型別不存在，
  詳見 [runtime-injection-status.md](runtime-injection-status.md:28) 與
  [runtime-injection-assets.md](runtime-injection-assets.md:67)。
- longyin／sanguo 的證據與定位尚未收入本檔，需由主調查合併後，才能形成完整 roadmap 參考表。
