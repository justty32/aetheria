# roadmap 參考盤點：社會、勢力與人物模擬

← [investigation](README.md)

## 問題

使用者要重排 roadmap，並指定《龍胤立志傳》與《英雄立志傳：三國志》作為社會模擬參考。本次只回答：

1. 兩款遊戲已查清哪些世界、勢力、人物、事件與時間推進規則？
2. 哪些做法值得放進 aetheria 的大中小三層規劃？
3. 哪些限制不能照搬，哪些地方仍沒有足夠證據？

## 結論

**兩個參考專案都已有可用的正式分析，但用途不同。**《龍胤立志傳》最適合借「固定世界骨架＋局部隨機」、「共同時間軸」、「事件分層」與「靜態定義／本局狀態分離」；《英雄立志傳：三國志》最適合借「決策每月、模擬每天」、「便宜的勢力目標評分」、「兩槽人物任務」與「人物定義／劇本狀態分離」。這些是 roadmap 的參考輸入，不代表 aetheria 已經實作，也不是本報告替 spec 裁定方案。

## 現況

### 《龍胤立志傳》

- **時間是一條共同的小時軸。**玩家行為先累積小時，滿 24 小時後依序推玩家、NPC、事件、勢力與背景工作；跨月另做集中結算。這適合拿來界定三層之間「一次行動怎麼推世界時間」，但實際 tick 粒度仍由 aetheria 後續規格決定。來源：[仿作向總覽](</home/lorkhan/repo/moddings/longyin/analysis/longyin/architecture/game-design-overview.md:7>)。
- **世界是固定骨架加局部隨機。**大地圖節點、旅行連線與區域 tile 骨架來自資料，開局才隨機建築落點、資源座標、道路外觀與治理值。這能支援「舊世界保持可辨識，但局部探索每局不同」。來源：[世界地圖生成](</home/lorkhan/repo/moddings/longyin/analysis/longyin/details/world-map-generation.md:3>)。
- **靜態定義與本局狀態分開。**地區、勢力、人物進存檔；功法定義不進存檔，人物只留定義 ID 與修煉進度；磁碟用 list，載入後才建 ID 字典。此做法可對應 aetheria 在舊世界追加文明、人物與物品定義。來源：[資料結構](</home/lorkhan/repo/moddings/longyin/analysis/longyin/details/data-structures.md:3>)、[存檔與索引](</home/lorkhan/repo/moddings/longyin/analysis/longyin/details/data-structures.md:63>)。
- **事件內容、觸發與結果處理分層。**每日生成一般事件與世界事件，玩家行為再觸發等待中的事件或任務；結果回寫人物、勢力、數值與劇情紀錄。純 AI 戰爭可直接算分，玩家涉入才進格子戰鬥。來源：[仿作向總覽](</home/lorkhan/repo/moddings/longyin/analysis/longyin/architecture/game-design-overview.md:64>)、[兩種戰爭](</home/lorkhan/repo/moddings/longyin/analysis/longyin/architecture/game-design-overview.md:88>)。

### 《英雄立志傳：三國志》

- **昂貴決策與日常模擬分頻率。**世界每天推城市、人物與出征隊；勢力出兵則每月重抽一個 1～28 日，只在當天計算一次。這是控制大量人物與勢力成本的直接參考。來源：[勢力 AI](</home/lorkhan/repo/moddings/sanguo/wf/workflows/investigations/faction-ai.md:18>)。
- **攻擊目標用簡單成本取最小。**成本只合併關係與君主義理、敵我戰力、城防；邊境分類代替完整尋路來估援軍。它提供的是便宜、可解釋的候選模型，不是必須照抄的公式。來源：[出兵評分](</home/lorkhan/repo/moddings/sanguo/wf/workflows/investigations/faction-ai.md:31>)。
- **人物是上層派工後的執行者。**每人只有「城市常駐工作＋外派任務」兩槽；勢力或城市每月挑人，人物每天只推進與結算。忠誠上限由關係與環狀相性決定，低忠誠不會自行叛變，必須有人主動密談。來源：[人物 AI](</home/lorkhan/repo/moddings/sanguo/wf/workflows/investigations/hero-ai.md:19>)。
- **固定人物與劇本狀態分離。**姓名、親屬、生卒與成長等放人物定義；勢力、城市、忠誠、官職與當局數值放劇本子物件，所以同一人物能套不同世界局勢。來源：[資料結構](</home/lorkhan/repo/moddings/sanguo/wf/workflows/investigations/data-structs.md:13>)。

## 差距

- 《龍胤立志傳》的 `HeroData` 把身份、地點、物品、AI、任務與關係塞在同一個大型聚合，aetheria 不應照搬成 C++ 巨型類別；原作也沒有自然增齡或老死，若要文明世代交替必須另立生命週期。來源：[人物聚合](</home/lorkhan/repo/moddings/longyin/analysis/longyin/details/data-structures.md:37>)、[生命週期缺口](</home/lorkhan/repo/moddings/longyin/analysis/longyin/architecture/game-design-overview.md:36>)。
- 《龍胤立志傳》的 284 個劇情專用處理器尚未逐一還原副作用，不能把已盤點的事件數量誤當成完整行為契約。來源：[未查支線](</home/lorkhan/repo/moddings/longyin/analysis/longyin/architecture/game-design-overview.md:119>)。
- 《英雄立志傳：三國志》把勢力 ID 直接等同君主 ID，會妨礙攝政、共治或勢力跨君主持續存在；aetheria 的文明／勢力身份不可綁死單一人物。來源：[人物 AI 限制](</home/lorkhan/repo/moddings/sanguo/wf/workflows/investigations/hero-ai.md:47>)。
- 該作另有「一支部隊在外就停掉整國內政外交」、玩家特判散落、機率未限界、缺錢任務永久停擺等問題，只能當反例。來源：[總覽的避坑](</home/lorkhan/repo/moddings/sanguo/wf/workflows/investigations/overview.md:32>)。
- 三國參考仍缺時間公式、城市建築決策、人物路徑天數、登庸率、部分存檔欄位等證據。來源：[共同缺口](</home/lorkhan/repo/moddings/sanguo/wf/workflows/investigations/overview.md:43>)。

## 牽扯到的部份

- **大層（Endless Legend 主軸）**：文明與勢力身份、外交／戰爭決策頻率、非玩家戰爭的抽象結算。
- **中層（已回歸原案城建與方陣戰場）**：地區連線、據點與資源歸屬、軍隊進入戰場前後的狀態交接。
- **小層（ToME4／RimWorld／Elin 主軸）**：只接收人物、任務、事件與戰鬥所需投影，結果必須回寫人物、勢力與地點；本報告不取代小層主參考。
- **跨層資料契約**：靜態定義與本局狀態、人物定義與劇本狀態、持久 ID，以及事件／戰爭結果的回寫邊界。
- **後續規劃判準**：需要另由 spec 決定時間粒度、人物自主程度、勢力是否獨立於領袖，以及哪些場景採抽象結算、哪些進入可操作地圖。
