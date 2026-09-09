# 活世界三層規劃：操作、呈現與可理解性

規則展開見 [spec 入口](../../../../../design/spec/README.md) 的 S11c 三層操作與可理解性；功能級草案已展開，仍待裁定與執行驗證。

[本層入口](README.md)｜← [plan](../../README.md)｜姊妹篇 [content](world-systems-content.md)

## 目標

讓玩家從全局決策、行軍與戰場指揮下沉到個人生活；縮放是看同一世界的不同解析度。畫面回答：
**我在哪、能做什麼、世界為何如此、情報多可信、代管犧牲什麼。**Godot 只呈現 core snapshot。

## 三層操作、地圖與資訊權威

| 層 | 主要操作 | 地圖 | 預設資訊 |
|---|---|---|---|
| Region | 行軍、外交、政策、拓殖、宣戰、關注事件 | 大陸與領土、道路、軍團、城市、威脅 | 勢力與軍團摘要、旬時間線、已知情報 |
| Site | 編隊、戰術、建造、生產、交易、組織管理 | 城鎮／戰場／聚落分區 | 隊伍、產業、庫存、士氣、局部事件 |
| Local | 走路、對話、使用物品／技能、工作、逐格戰鬥 | 室內外 tile、人物、物件、視線 | 身體、需求、裝備、關係、眼前感知 |

事件主場與 zone 是權威；畫面只拿 id、可見投影與 command schema。三層各一套場景與鏡頭，
保留空間關係做縮放轉場；不得再共用除錯 TextureRect。
[現況證據](../../../investigation/godot-pipeline-status.md)｜[駐留要求](../../../../../design/maps/player-residence.md)

## 知識、迷霧與資訊來源

Site 平時顯示工作阻塞、服務覆蓋與委任；戰時突出方陣朝向、隊形、戰線與命令結果。切換的是資訊重點，不換一張無關的城市。畫面士兵不等於核心逐兵實體，詳見 [方陣指揮](../site/world-site-formations.md)。

「發生」與「知道」分開。資訊帶 `subject、觀測 tick、來源、位置精度、可信度、過期規則`。
視線管 Local；探子、商隊、傳聞、信件與外交提供遠方知識。舊情報明示年代，不偷更新。

- Region 顯示已知領土與估計兵力；Site 顯示在場、所屬或有報告的產業／人物；Local 只顯示感官
  與角色知識。私人知識隨角色檔，共享情報住世界，合併仍保留來源。

## 人物生活、事件與歷史

人物面板回答「做什麼、為何、需要什麼、和誰有關係」。工作鏈是需求／命令→goal→job→step→結果；
資源被誰占用要能查，健康、需求、技能、心智、工作、關係分開摘要。RimWorld 僅作責任參考，
不搬 C# 結構。[參考](</home/lorkhan/repo/moddings/rimworld/analysis/rimworld/architecture/ai_deep_dive.md:5>)

同一事件 id 有三種畫面：Region 圖標、Site 隊伍／階段、Local 人物行動。歷史保存
「原因→決定→結果→消息來源」，可按人物、地點、勢力、物品篩選；未知不得洩漏。

## 狀態、輸入、輸出與畫面時序

1. Godot 送 command 與視角 id，core 回 accepted／rejected＋理由。
2. core 結算並發布 revision：tick、駐留層、知識版本、事件差分。
3. Godot 只取目前層的 snapshot＋delta；列表虛擬化、地圖分塊。
4. 重要事件進通知；是否中斷旅行由 core 依 significance 門檻判斷。
5. 下沉先顯示已知摘要與載入進度，再用確定性投影換成細節；上提先由 core 歸約成功才切畫面。
6. 存讀、切角色、代管切換都等待 commit point；Godot 不保存 pending 玩法狀態。

## 代管與切換

駐留層是玩家選擇，observer 是系統算多細。玩家可把部隊、城市、人物交給代理策略；代管是 core AI
產生同一 command，列目標、禁令、資源上限、中斷條件、報告頻率。可收回，但已提交行動不倒帶。

遭遇可在 Region 結算、Site 指揮、Local 親戰；三路期望值一致，低層爭取 δ。重大事件依門檻中斷，
日常完成只進摘要。
[既有裁定](../../../../../design/maps/player-residence.md)｜[observer](../../../../../design/simulation/observer.md)

## 規則跨系統如何被看見

- 文化改工作與外交：用可見的習俗、需求與承諾解釋原因，不顯示未知敵方權重。
- 經濟短缺→Site 工作→Local 搬運→Region 價格：同一因果鏈，不做三套通知。
- 戰爭→戰場→傷亡用同一事件 id 串起軍報、戰報、人物史。
- 注入包顯示依賴、影響與驗證；生效後記加入 tick。玩法資料 fail-fast，缺素材用明顯 fallback。

## 美術、音景與內容製作

`VisualRef/AudioRef/TextRef` 指資產；core 送語意 id、狀態、種子，Godot 組畫面音效。三層共享視覺
語彙，讓 Region 森林、Site 林地、Local 樹看得出同一地方；狀態標籤驅動變體。

音景分氣候、Site 活動、Local 聲源、事件提示；切層淡化。製作者用 schema 驗證的文字檔製作 def、
條件、效果、任務、對話、素材與翻譯；固定 seed 預覽不得另做玩法模擬。

## 調查與除錯工具

世界模擬器必須可問「為什麼」：

- inspector：zone／事件／人物的權威層、LOD、observer、更新 tick、信箱。
- AI trace：候選 goal、分數、拒絕、reservation、最後 command。
- knowledge overlay：真值與角色已知值並排；正式遊玩禁用真值。
- replay：依 seq 看 command/outcome、hash、首個分歧；只讀正式日誌。
- 生成探針：seed、階段、邊界、包來源；輸出可比較摘要。
- 呈現診斷：遺失 VisualRef、fallback、髒區、snapshot 成本。

工具只走 core 唯讀查詢或離線 replay，不把作弊狀態塞進 Godot。[呈現調查](../../../investigation/art-pipeline-status.md)

## 來源與界線

- TES／騎砍／Civ／DF 定的是體驗目標，不授權照搬 UI、公式或資產。
- ToME4 可借探索、技能／物品／任務的內容密度；RimWorld 可借人物工作與局部狀態可解釋性；
  Elin 目前只有早期 analysis，可借生成／行為／Drama 的分工線索，不能當驗收事實。
- Endless 等大層參考負責文明策略，不應把全知 4X UI 帶進角色視角；所有顯示仍受知識系統約束。

## 驗收

1. 同一戰爭可在三層間切換，地點、部隊、人物、傷亡、時間與事件 id 連續，無重複結算。
2. 只靠正常 UI 可完成旅行、外交、城鎮命令、Site 戰鬥、Local 探索、存讀與切角色。
3. 兩角色看同一遠方事件得到不同且可解釋的資訊；讀檔後知識來源、可信度與過期時間一致。
4. 代管城市跑 100 旬後可逐項回答 AI 為何行動；中斷門檻能擋噪音且不漏重大事件。
5. 關掉 Godot 重開，從 core 重建三層畫面與音景一致；Godot 暫存全清不改世界 hash。
6. 新內容包加入後，其地圖、人物、技能、物品、事件、文字與音景可被定位；缺依賴整包拒收。
7. 大世界不用載入所有 Local；pan／zoom／切層與事件高峰仍達成後續裁定的 frame、記憶體、延遲預算。

## 待細化

- 三層 HUD 與鏡頭操作、Region→Local 是否可跳層、選中多部隊時的駐留語意。
- 私人／組織知識合併、欺情報與更正、戰爭迷霧的精度與遺忘規則。
- 代管策略的可調欄位、重大事件預設門檻、多角色控制交接；本次不規劃連線多人。
- 美術規格、音景 taxonomy、內容編輯器形式與效能數字；都要量測後裁定，不能先拍常數。
