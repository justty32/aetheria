# 領域覆蓋與收斂複核

← [本層入口](README.md)｜[主文](spec-world-review.md)

## 原框架的其他部分沒有漏掉

已重讀 [十二系統框架](../../../wf/workflows/plan/living-world/world-framework.md) 與 [原需求對照](../../../wf/workflows/plan/living-world/roadmap-coverage.md)。上表涵蓋政治、人物、生活、戰爭與 AI；其餘接點如下。

- **環境與生態：**[環境](../nature/spec-environment.md)讓工程改變通水與地貌；[生態](../nature/spec-ecology.md)由棲地、覓食與避險接到獸群遷移，再改牧人安排；[災害](../nature/spec-disasters.md)把預警、實際損失、疏散和重建分開。水退、驅離和離場都不自動復原。
- **經濟與旅行：**[市場](../economy/spec-markets.md)把需求、訂單、真實交貨、部分履約和欠款串起；[旅行](../economy/spec-travel.md)有集合、受阻、改道、拆隊、到達與交接。承諾不等於到貨，受阻仍有消耗。
- **力量、信仰與知識：**[奇幻手段](../common/cases/spec-world-magic.md)比較傳送、感知、治療、召喚與造糧如何改供應及部署；[教團](../magic/spec-faith.md)有神力失效後的合作、縮減和轉送；[知識](../magic/spec-knowledge.md)把發現、理解、使用、教學與失傳分開，導師或設備失去後能重議。
- **探索、歷史與故事：**[探索](../adventure/spec-exploration.md)讓居民持續生活、發現物實際運出、離場保留改變；[生成史](../world/history/spec-history.md)從初始來源接年代、遺跡及日常；[故事](../adventure/spec-stories.md)由真需求形成可失敗的委託，文字不替世界發獎或保人物不死。
- **內容與續存：**[內容追加](../content/spec-content.md)保留全 def 類型、整批驗證、回合尾提交、舊規則與歷史，新人物接第一個合法決策點；[腳本](../content/spec-scripting.md)保留 M11 的受限效果、階段、失敗與重放。M10 與 M11 不合併縮水。
- **呈現與原開發驗收：**M0 純核心／Godot 分工保留；M1–M11 的實跑、主線、美術、音景、全 def 使用矩陣和校準仍是開發要求。當前未被授權實作，玩家呈現依最新指示後置。mark 與 root 的擱置也沒有解除。

## 收斂後再次核對

2026-09-10，對照收斂提交 `99d448e` 與前批審閱 `6c5a5ba`：領域正文未變，改的是入口、交接與 [短版主線](../overview/spec-world-core.md)。本輪重讀原框架及三層、生成接手、分檔、自然、旅行、知識與故事正文，另審軍隊、城區、人物、家庭、等待、情報及敵我自主；上述逐項證據仍成立。修正河谷篇誤稱和平／衰退與主動生活尚未展開的舊提示。

短版保留選擇理由、失敗後果及接續，沒有用摘要取代領域規則。本結論限當時的共用流程與收斂；使用者後續要求的勢力差異、神明自主及紀元變革另沿 [交接](spec-review-next.md) 深化。數值、玩家體驗與實作驗證仍後置。
