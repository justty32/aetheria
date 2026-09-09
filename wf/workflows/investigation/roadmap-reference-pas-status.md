# roadmap 參考盤點：PAS 的 CK2、太閤立志傳 V、LDE3

← [investigation](README.md)｜2026-09-09

## 問題

使用者要重排三層 roadmap，另盤點 PAS 的 ck2、taiko、lde3。中層其後回歸原案；最新定位見 [藍圖](../plan/living-world/roadmap-three-layers.md)。

可判定提問：①三份既有分析各有哪 2～4 個有證據、值得借的系統或架構？②它們補的是哪種
跨層能力，而不是取代三層主骨架？③哪些結論只是逆向推測或受原作工具限制？④名稱是否正確？

## 結論

**三者適合補「世界社會怎麼活、任務怎麼串、遠方模擬怎麼省」，不適合改寫三層主參考。**

- 實際目錄是 `crusader-kings-2`、`taikou`、`lde3`；索引正式名稱是《太閤立志傳 V
  (Taikou V)》，不是 `taiko`（[PAS 索引](</home/lorkhan/repo/pas/index/engines-and-games.md:20>)）。
- CK2 補政治關係、生命週期事件與共用加權規則；太閤補人物—職位—任務—勢力演化；
  LDE3 補有限任務原型與世界／子地圖內容串接。
- 這些是 roadmap 的參考證據，不是 aetheria 已有功能，也不是方案裁定。

## 現況

### CK2：社會不是一張勢力表，而是一張會改變的關係圖

1. **角色持有職位，職位組成勢力。** CK2 沒有獨立「國家」物件；頭銜持有、封臣關係與
   法理／實控兩套疆域共同表達分裂、繼承與篡奪。值得借的是「人物、職位、領地」可分開換手，
   不必把國家做成不可拆的大物件
   （[資料模型](</home/lorkhan/repo/pas/analysis/crusader-kings-2/architecture/02_data_structures.md:20>)、
   [法理與實控](</home/lorkhan/repo/pas/analysis/crusader-kings-2/architecture/02_data_structures.md:80>)）。
2. **引擎只報時機，內容決定反應。** 出生、死亡、易主、戰勝、月／年脈動由 `on_actions`
   接到事件；背景事件另用 MTTH 的基準時間乘條件權重。這能讓大層政治、中層戰果、小層人物事件
   共用回寫路徑（[生命週期掛鉤](</home/lorkhan/repo/pas/analysis/crusader-kings-2/architecture/03_world_simulation.md:31>)、
   [MTTH](</home/lorkhan/repo/pas/analysis/crusader-kings-2/architecture/03_world_simulation.md:49>)）。
3. **少量正交原語拼內容。** Scope／Trigger／Effect／Modifier 加上命名條件與命名效果，
   同一格式可支撐事件、AI 與規則例外；值得借的是共用可解釋的語彙，不是照搬 DSL
   （[四原語](</home/lorkhan/repo/pas/analysis/crusader-kings-2/architecture/01_overview.md:28>)、
   [腳本層抽取](</home/lorkhan/repo/pas/analysis/crusader-kings-2/details/04_events_decisions_and_script_state.md:236>)）。

### 太閤立志傳 V：用少量人物參數讓勢力自己分裂

1. **靜態母體、開局劇本、執行期存檔分開。** 固有資料跨劇本共用；所屬、位置與外交是
   劇本狀態；遊玩變化再進存檔。這是定義與世界實例分家的參考
   （[三層資料](</home/lorkhan/repo/pas/analysis/taikou/architecture/00-overview.md:20>)、
   [設計摘要](</home/lorkhan/repo/pas/analysis/taikou/architecture/00-overview.md:45>)）。
2. **背景勢力錯峰結算。** 不同勢力依 ID 分散到不同評定日，避免同一天全世界一起算；
   可借給遠方勢力低頻 tick，但 ID 只能當穩定相位種子，不能成為玩法規則
   （[排程公式](</home/lorkhan/repo/pas/analysis/taikou/architecture/05-npc-behavior.md:57>)）。
3. **方針先縮小任務池，再按身分派工。** AI 不必逐人求全域最優；先選內政／軍備等方針，
   再以職位過濾可接主命者，完成與回報才結算功勳
   （[主命分派](</home/lorkhan/repo/pas/analysis/taikou/architecture/05-npc-behavior.md:78>)）。
4. **野心、義理、相性與忠誠形成離反鏈。** 低忠誠等硬條件加權後，城主可自立或投靠別家，
   直接使人物關係回寫為大層勢力分裂
   （[離反判定](</home/lorkhan/repo/pas/analysis/taikou/architecture/05-npc-behavior.md:170>)）。

### LDE3：用有限任務骨架組合內容

1. **任務原型固定，內容資料替換。** 18 種任務狀態機涵蓋送貨、護衛、暗殺、探索、逃亡等；
   多筆記錄能串成階段任務。值得借的是「少量骨架＋資料參數」
   （[型態表](</home/lorkhan/repo/pas/analysis/lde3/architecture/20-event-script-engine.md:73>)、
   [任務鏈](</home/lorkhan/repo/pas/analysis/lde3/architecture/20-event-script-engine.md:180>)）。
2. **結構與台詞分離。** SBD 管型態、參數與階段；STD 管固定語意槽及人物／物品／目的地
   佔位符。這讓同一流程能換角色、地點與文字
   （[SBD／STD 對應](</home/lorkhan/repo/pas/analysis/lde3/architecture/20-event-script-engine.md:121>)）。
3. **總圖只保存入口，細節在子圖。** 已確認世界圖以區域拼貼，城鎮用座標與 Town ID 定位，
   地城另有編號檔；可作為「上層節點指向下層內容」的研究候選
   （[拼貼層](</home/lorkhan/repo/pas/analysis/lde3/architecture/50-maps-dungeons.md:81>)、
   [子圖入口](</home/lorkhan/repo/pas/analysis/lde3/architecture/50-maps-dungeons.md:112>)）。

## 差距

1. **CK2 證據主要來自可讀內容層。** 引擎是 stripped binary；後續深挖更明說原安裝目錄當時
   不可讀、未補原檔行號，因此 tick 執行順序、效能與內部資料結構不能當已證實
   （[來源限制](</home/lorkhan/repo/pas/analysis/crusader-kings-2/details/04_events_decisions_and_script_state.md:7>)）。
2. **太閤來源跨版本。** 欄位主幹取自 2022 DX 編輯器，2004 PC 版偏移不同；語意可參考，
   二進位布局不可照搬（[版本限制](</home/lorkhan/repo/pas/analysis/taikou/architecture/02-characters-items.md:5>)）。
   開戰／選城、行動日議程、NPC 外交仍是黑箱，而且原作 CPU 內政循環有已知缺口
   （[黑箱清單](</home/lorkhan/repo/pas/analysis/taikou/architecture/05-npc-behavior.md:19>)、
   [內政缺口](</home/lorkhan/repo/pas/analysis/taikou/architecture/05-npc-behavior.md:135>)）。
3. **LDE3 只能確信格式輪廓。** 靜態容器解壓器尚未完成，存檔、勢力、軍隊與地城逐格 layer
   都未解（[進度](</home/lorkhan/repo/pas/analysis/lde3/architecture/README.md:22>)）。任務參數 A/B/C 的
   精確語意、命令槽 opcode、世界三層模型與地圖事件層都仍是推測
   （[未解欄位](</home/lorkhan/repo/pas/analysis/lde3/architecture/20-event-script-engine.md:191>)、
   [世界模型信心](</home/lorkhan/repo/pas/analysis/lde3/architecture/30-world-state-factions.md:159>)）。
4. 三份分析都不是 aetheria 現況證明；本案是否已有等價物尚未核對。

## 牽扯到的部份

- roadmap 定位：CK2 放「政治關係＋事件規則」；太閤放「人物成長／派工／勢力裂變＋遠方排程」；
  LDE3 放「任務內容格式＋上層入口指向子地圖」。三者都是補充欄，不替代既定大／中／小主參考。
- 後續 spec 才裁定身分、所有權、tick、任務狀態、事件 target、權重與存檔；本檔不定方案。
- 若採資料驅動事件，測試與除錯判準必須能說明：由哪個入口觸發、條件為何通過、改了哪些狀態、
  跨層引用指向誰；CK2 分析也把「為何觸發／未觸發」列為主要風險
  （[除錯提醒](</home/lorkhan/repo/pas/analysis/crusader-kings-2/details/04_events_decisions_and_script_state.md:288>)）。
- 若採錯峰模擬或任務鏈，還會碰確定性、存檔版本、事件順序與世界雜湊判準；不能直接採用
  原作的 ID 日期公式、固定二進位槽或未解壓的資料布局。
