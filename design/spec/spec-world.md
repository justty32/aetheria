# 三層活世界：全批規格總覽

← [本層入口](README.md)｜來源：[roadmap](../../wf/workflows/plan/living-world/roadmap-2026-09.md)｜[原則](../principles.md)｜[大綱](../outline.md)

## 狀態與完成定義

2026-09-09，使用者要求一路完成規格；**S0–S11 功能級規格草案已全批展開**，並補個體交戰與完整戰役。只改文件，未實作、未做遊戲執行驗證、未將建議標成使用者裁定。

**Done when：**十二系統與三層共同契約都有資料責任、輸入／輸出、時間、命令、失敗／中斷、AI、保存與可證偽情境；跨系統鏈能逐項對帳；舊文衝突、待裁定與未來驗證可定位。全批達成的是這個文件完成條件，不是所有內容表、平衡或 API 已定。

上述是前輪全批草案的盤點標準。2026-09-10 使用者更正：**先把三層世界與 NPC 自主運作串完整，再把玩家接成由使用者提供決策的 NPC。** 現在從 [共用流程](common/README.md) 進入，玩家體驗文件後置；仍不要求先定完公式與罕見例外。

**本輪高層流程核對：**已按下表逐組檢查 S0–S11 的決策者、行動、失敗、結果與下一輪承接。生成接手、城鎮生活、經濟、政治、戰爭、自然、野外居民、奇幻力量、情報與追加內容皆有落點；七個原情境另以純 NPC 串接。這表示「大概完整的世界運作草案」已具備，不表示所有 NPC 情境已窮舉，亦不表示實作、平衡或粗細校準通過。細則仍按需要選，不以新增同義文件充當進度。

使用者後續要求繼續四塊行為；前述核對只證明責任鏈有接手者。勢力打算、軍隊調度、城區發展與人物取捨仍在展開，不能以這次核對宣稱整體規劃已完成。

下一層導引：[整合推演](spec-integration.md)、[裁定清單](spec-decisions.md)、[驗證矩陣](spec-verification.md)、[交接與後續](spec-review-next.md)。數值不確定不影響讀懂規則；依 [規劃尺度](spec-planning-scope.md)，一般取捨由 agent 按需收斂，細則不過早定死；實作仍須另獲指示。

## 規格依賴與閱讀順序

| 編號 | 規格 | 負責回答 |
|---|---|---|
| S0 | [身分與交接](common/spec-world-handoff.md) | 同一對象、模擬配額與提交一次 |
| S1 | [時間與提交](common/spec-world-time.md) | 生效、中斷、跨旬、存讀與重放 |
| S2 | [地方工作與服務](site/spec-site-work.md) | 派工、領料、交付、取消與接手 |
| S3 | [方陣命令](site/spec-site-battle.md) | 占地、朝向、接敵、失控、撤退 |
| S3b | [個體交戰](spec-local-combat.md) | 動作、潛行、救援、傷害與上層回傳 |
| S4 | [平戰連續性](site/spec-site-transition.md) | 同一城市動員、圍困、失守與恢復 |
| S4b | [戰役與軍需](spec-campaigns.md) | 補給、圍城、多方／海空、俘虜及復員 |
| S5a | [人口與人物](people/spec-population.md) | 群組、具名、遷徙、生死與命運赤字 |
| S5b | [家庭與職涯](people/spec-households.md) | 關係、照護、任職與承諾 |
| S5c | [需求與健康](people/spec-life-health.md) | 消耗、失能、照護、恢復與死亡 |
| S6a | [物資與物品](economy/spec-goods.md) | 拆批、製作、腐敗、品質與保管 |
| S6b | [市場與履約](economy/spec-markets.md) | 訂單、交貨、支付、信用與違約 |
| S6c | [旅行與運輸](economy/spec-travel.md) | 在途、改道、容量、遭遇與切層 |
| S7a | [政體與委任](spec-government.md) | 組織、職位、代表權與預算 |
| S7b | [法律與司法](spec-law.md) | 法源、財產、徵收、證據與執行 |
| S7c | [繼承與分合](spec-succession.md) | 職位、遺產、照護與義務接手 |
| S7d | [外交與和解](spec-diplomacy.md) | 條約、戰爭、叛亂與分裂承接 |
| S8a | [環境與水文](spec-environment.md) | 水、污染、天氣及跨界改造 |
| S8b | [生態與耗竭](spec-ecology.md) | 生長、採捕、農牧林礦與復育 |
| S8c | [災害與恢復](spec-disasters.md) | 預警、傳播、暴露、救援及重建 |
| S8d | [歷史與聚落](spec-history.md) | 多年代興衰、證據及開局接手 |
| S9a | [力量與修行](spec-powers.md) | 取得、施用、維持、失去與文明影響 |
| S9b | [知識與研究](spec-knowledge.md) | 理解、實作、教學、載體與失傳 |
| S9c | [信仰與教團](spec-faith.md) | 誓約、制度、神蹟與眷顧失效 |
| S9d | [探索與發現](spec-exploration.md) | 遠征、遺跡、取回、耗竭與公布 |
| S10a | [情報與傳聞](spec-information.md) | 真值、知情、轉述、更正與保密 |
| S10b | [立場與文化](spec-motives.md) | 不同價值、關係、記憶與文化傳播 |
| S10c | [三層 AI](spec-ai.md) | 目標、候選、預算、派工及失敗回饋 |
| S10d | [玩家身分與委任](spec-player-agency.md) | 人生角色、控制交接、代管與續玩 |
| S10e | [任務與敘事](spec-stories.md) | 真需求、對話、分支、競爭與結清 |
| S11a | [內容與版本](spec-content.md) | 全 def、實體與包的追加及相容 |
| S11b | [腳本與製作](spec-scripting.md) | 原語、權限、沙箱、預覽及錯誤 |
| S11c | [三層呈現](spec-presentation.md) | 正常操作、解釋、畫面／音景與可及性 |

S0／S1 是共同依賴；S2／S3／S3b 由 S4／S4b 接成平戰生命週期。S5–S8 提供人物、物資、制度與自然；S9 會反向改變這些條件；S10 在各領域合法命令上決策／敘事；S11 貫穿規則追加及呈現。此為規格依賴，不是施工排程。

## 需求覆蓋與參考

上游 [十二系統框架](../../wf/workflows/plan/living-world/world-framework.md) 與 [原需求覆蓋](../../wf/workflows/plan/living-world/roadmap-coverage.md) 保留原始範圍：

| 框架系統 | 規格落點 |
|---|---|
| 地理／生態 | S8；自然效果交到 S5／S6 |
| 經濟／城建 | S2／S6，平戰接 S4 |
| 文明／政治 | S7，文化 S10b、宗教 S9c |
| 人物／家庭 | S5，動機 S10b、人生 S10d |
| 生活／工作 | S2／S5／S6，個人互動 S3b／S9d |
| 力量／知識 | S9，製作 S6a、版本 S11a |
| 戰爭 | S3／S3b／S4／S4b／S7d |
| 旅行／探索 | S6c／S9d |
| 歷史／故事 | S8d／S10a／S10e |
| 三層 AI | S10c／S10d，加各領域自身條件 |
| 內容／續存 | S0／S1／S11a／S11b |
| 操作／美術音景 | S11c，知情 S10a、控制 S10d |

Endless 承擔大局參考；中層仍原案地方經營＋Total War 式回合制方陣；ToME4／RimWorld／Elin 補探索生活。其餘指定作品補人物、制度、知識、任務；證據成熟度沿各篇所指調查，不因被納入規格而變成已驗證可直接搬用的實作。

## 審閱與實作邊界

舊文的旬末上繳、易失 HP／狀態、EntityRef、命運赤字、位階／重要性、全知大事件、地城重生、AI 國力曲線、素材缺失政策均已集中到裁定清單及對應葉文件，未暗改原意。現在程式能力只依 [實作基線](../../wf/workflows/investigation/roadmap-baseline-status.md)，不以舊里程碑或草案數量代替證據。

本輪不包含：程式與 API、存檔格式升版、全量內容及平衡數值、素材製作、實機校準、實作任務書。2026-09-10 使用者已授權持續規劃及 push，文件驗證後可提交同步。mark／獨特物件追蹤與 root 成長仍擱置；一般穩定身分與故事歷史不因此停擺。
