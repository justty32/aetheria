# 活世界系統草案：人物

[本層入口](README.md)｜← [世界框架](../world-framework.md)｜[狀態分工](../world-state-ownership.md)｜[用詞](../../../../../design/glossary.md)

## 目標

每個人從出生、成長、成家、任職到死亡都活在同一世界；玩家不是例外型別，只是由人輸入意圖的
人物。NPC 與玩家共用需求、能力、移動、交易、犯罪、戰鬥與後果規則，差別只在誰選行動。
世界可有數十萬人，但只細算值得細算者；不可把完整願景縮成幾個示範 NPC。

## 三層各做什麼及唯一權威

| 層 | 做什麼 | 唯一權威 |
|---|---|---|
| 大層 | 人口 Cohort、遷徙、出生死亡、階級／文化／宗教分布、王朝與名人摘要 | Region 的人口與組織帳本 |
| 中層 | 軍隊、家族、行會、宮廷、城鎮職缺；具名領袖與隊員的旬級行動 | Site／軍團的名冊、職位與排程 |
| 小層 | 單人位置、日程、對話、工作、戰鬥、物品與短期狀態 | 已具現人物的 Local 狀態 |

同一人物只有一個 `PersonId`。低層是高層紀錄的展開，不另生一個「戰鬥版角色」真相；降格時把
存活、傷勢、關係、財產與事蹟歸約，HP、路徑、動畫丟棄。同層互動一律經所在地、組織、交易或事件
中介，不讓未載入人物互相持有指標。

## 狀態／輸入輸出／時間

- 持久狀態：出生與血統、身分、家庭關係、文化信仰、特質、能力底值、知識、職歷、財產、重傷、
  誓約、仇怨、事蹟、死亡原因。名字一旦生成即持久。
- 持久位置：所屬 zone、旅行區段、事件主場身分；局部格位置、路徑可由主場與 seed 重建。
  疲勞、情緒、短期效果只可丟棄能由持久負擔、事件與到期時間重建的呈現；不能換層就消除代價。
  外貌細節、未承諾日程與無名者名字可程序生成；既有承諾與已具名者不重抽。
- 輸入：生理與安全需求、關係請求、組織命令、附近機會、人格／價值觀、玩家命令、世界事件。
  輸出：行動意圖，交由共用 action resolver 驗證成本與後果。
- 人物、生活、力量各自由所屬領域規則改狀態；AI 只提意圖，不擁有健康、物品或關係的寫入權。
- 三層沿用既定探索／戰鬥步長；人物決策可低頻喚醒，速率按共同秒制時間。長行動存開始、進度與期限。
  生死事實在合法階段確認；實體建立銷毀、組織結構變更仍排回合尾，不因加速漏算。

## 核心規則與跨系統因果

1. AI 先選「為自己、家庭、友人、組織、信仰、仇敵」哪種動機，再選目標與可執行行動；需求久未
   滿足會升權重，硬規則要明寫，失敗要保存原因。玩家直接選目標，仍過同一 resolver。
2. 關係分開保存親屬／婚姻／師徒等身分、單向感情、信任、人情債與已知情報。互動產生事件，事件
   同時更新雙方；殺人可沿親友與組織傳成復仇、繼承與外交。
3. 家庭是生育、照護、繼承、婚盟、家產與姓氏的單位；職業由能力、知識、工具與職缺形成，不用
   永久職業 class。人可兼具家庭、軍隊、行會、宗教與政權職位。
4. 組織有獨立 ID、章程、職位、資產、領地、聲望、政策與接班規則，不能把勢力 ID 借用君主 ID。
   換領袖會改 AI 傾向，但組織歷史與債務延續。
5. significance 決定個體或 Cohort 計算。災害先算 Cohort 權威總量，再判具名人物命運並扣抵配額；
   升降格要遲滯、可校準、同 seed 同命令同結果。

因果例：歉收→糧價與失業→家庭遷徙／盜匪 Cohort→商路受阻→領主徵兵清剿→個人傷亡、遺孤、
仇怨與繼承→後續任務和外交，而不是各系統各自吐隨機事件。

## 既有／新增建議

既有原則可直接保留：高層權威與守恆、三類資料、significance／Cohort、事件升降格、確定性
（[principles.md](../../../../../design/principles.md)、[significance.md](../../../../../design/simulation/significance.md)、
[significance-fate.md](../../../../../design/simulation/significance-fate.md)）。

新增候選：`PersonRecord`、`Household`、`Organization`、`RoleAssignment`、`RelationshipEdge`、
`LifeEvent`、動機／目標／行動 def、跨層 person projection／reduction。先定語意與守恆測試，再拆輪次，
不塞進 `PlayableSession`。mark 仍是擱置裁定；若以玩家關係自動提高人物解析度，實作前必須另行解除
mark 擱置，不能由本草案暗改。

## 來源與證據限度

- 《鬼谷八荒》提供動機→目標→行動、跨人請求與復仇鏈的靜態證據
  （[npc-ai.md](</home/lorkhan/repo/moddings/guigu/wf/workflows/investigations/npc-ai.md:17>)）；長期分布未實測。
- 《覓長生》證實輕量 NPC 紀錄到戰鬥投影，但宗門只是標籤，不可當勢力範本
  （[data-structs.md](</home/lorkhan/repo/moddings/michangsheng/wf/workflows/investigations/data-structs.md:5>)）。
- 《修仙模擬器》提供遠方人物存 seed、需要才生成；遠方人物不成長是反例
  （[time-progression.md](</home/lorkhan/repo/moddings/xiuxian/wf/workflows/investigations/time-progression.md:43>)）。
- sanguo 顯示忠誠、職涯、繼承可由人物關係驅動，但人物自主性很低
  （[overview.md](</home/lorkhan/repo/moddings/sanguo/wf/workflows/investigations/overview.md:9>)）。
- CK2／太閤材料只作家族政治與個人職涯方向參考；本輪未重新驗證遊戲程式，不能把分析推論當公式
  （[CK2](</home/lorkhan/repo/pas/analysis/crusader-kings-2/architecture/04_npc_ai.md:1>)、
  [太閤](</home/lorkhan/repo/pas/analysis/taikou/architecture/05-npc-behavior.md:1>)）。

## 驗收情境

一名無名農戶在饑荒中遷城、從軍、立功成為具名軍官，結婚生子後戰死。大層人口與軍力全程守恆；
中層名冊、薪餉、家庭收入同步；玩家趕到戰場可在小層改變其命運。卸載重載後姓名、家族、傷亡、
遺產、仇怨與史錄一致；玩家若走同樣命令，結果位元一致。玩家角色改由 AI 接管也能按同規則生活。

## 待細化

- 人口 Cohort 分布、出生死亡與跨族婚生規則；家庭財產與各政體繼承法。
- 動機權重、人格維度、關係衰減／傳播、同意與背叛規則。
- 組織類型、職位市場、升遷罷免、多人共治與法人責任。
- 人物升降格的欄位映射、無名人成名與退場預算；需 mark 的部分另案裁定。
