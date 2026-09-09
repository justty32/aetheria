# 世界規劃的參考總盤點

← [調查入口](README.md)｜[規劃對照](../plan/living-world/roadmap-coverage.md)｜2026-09-09

## 問題

使用者要先規劃三層奇幻世界模擬器，充分參考 `~/repo/moddings` 與 `~/repo/pas/analysis`。哪些材料有用？哪些已被核對，哪些仍只是分析或構想？

本次讀本地文件、索引與部分本案程式碼，不運行參考遊戲、不另做完整逆向。報告描述「研究材料提供什麼」，不把材料中的建議冒充原作事實。

## 結論

十四個指定參考已有對應盤點，另挑六份 PAS 材料作補充；不足以照抄所有公式與 API。最新分工：大層 Endless，中層回歸原案城建與 Total War 式回合制方陣，小層 ToME4／RimWorld／Elin。VCMI／Wesnoth 轉為輔助參考；Total War 定位依本案既有設計，未新增原作逆向證據。

## 現況

### Endless：大層的主要底座

已讀 [4X 藍圖](/home/lorkhan/repo/moddings/endless/wf/workflows/investigations/4x-design.md:5) 與四份子題、原作回合／AI／省份分析。可用內容：

- 定義、狀態、命令、效果、事件與查詢分開；玩家與 AI 同命令入口，改動可追查來源。這是研究綜合出的可移植邊界，並非原作現成 C++ API。
- [回合經濟](/home/lorkhan/repo/moddings/endless/wf/workflows/investigations/4x-turn-economy.md:15) 把立即、回合末、跨回合後、開放輸入前分開，並提出依賴圖與批次收成；原作流程見 [turn-loop](/home/lorkhan/repo/moddings/endless/wf/workflows/investigations/turn-loop.md:7)。本案採其明確時序的原則，不換掉秒制時鐘。
- [AI](/home/lorkhan/repo/moddings/endless/wf/workflows/investigations/4x-ai.md:17) 提供感知、目標、候選、資源分配、命令與回饋；適合補上本案 AI 真正對世界行動的接線。
- [資料與世界](/home/lorkhan/repo/moddings/endless/wf/workflows/investigations/4x-simulation.md:63) 分開領土控制、採收範圍與戰略點；[衝突社會](/home/lorkhan/repo/moddings/endless/wf/workflows/investigations/4x-conflict-society.md:57) 區分正式關係、談判、條約與態度。這些能讓經濟政治共用地圖而不混成一個 owner 欄位。

重要限制：原作的省份、客戶端／伺服器、一省一城和戰鬥子模擬不等於 aetheria 的三層。研究中的「多人幾乎免費」等評語不採作工程承諾；本次沒有規劃多人功能。

### 其餘指定參考

| 材料 | 已讀範圍與結論 | 細節 |
|---|---|---|
| VCMI／Wesnoth | 戰場建立、兵力身分、出手、地形、AI、戰果回寫；兩作行動制不能混用 | [中層報告](roadmap-reference-tactics-status.md) |
| ToME4／RimWorld／Elin | 探索、工作、人物、區域生命週期；區分已核對知識與早期分析 | [小層報告](roadmap-reference-local-status.md) |
| guigu／michangsheng／xiuxian | 人物目標、關係、月結與粗算；外界模擬深度差異很大 | [人物報告](roadmap-reference-people-status.md) |
| longyin／sanguo | 人物職責、勢力派工與日月因果；不搬巨型類別與人物勢力共用 ID | [社會報告](roadmap-reference-society-status.md) |
| CK2／taikou／lde3 | 政治圖、繼承、職涯、任務鏈；黑箱與跨版本證據要保留標記 | [PAS 報告](roadmap-reference-pas-status.md) |

### PAS 其他可用材料

以下讀了正文，不只看索引。仍屬本地分析，本次沒有重新跑原作驗證。

| 來源 | 提供的具體參考 | 限度 |
|---|---|---|
| [Freeciv 生成](/home/lorkhan/repo/pas/analysis/freeciv/details/mapgen_deepdive.md:8)、[外交運輸](/home/lorkhan/repo/pas/analysis/freeciv/details/ai_diplomacy_logistics.md:5) | 分階段生成；船與乘客供需、外交態度變化 | 本文有來源函式，參數仍需按本案校準 |
| [Unciv 回合](/home/lorkhan/repo/pas/analysis/unciv/details/ai_turn_logic_flow.md:7)、[決定性](/home/lorkhan/repo/pas/analysis/unciv/details/fixed_point_and_determinism.md:27) | AI 明確執行順序、危機覆寫、移動與亂數穩定性 | 不能把分析裡的 C++ 重構建議當 Kotlin 原作實作 |
| [Veloren 世界生成](/home/lorkhan/repo/pas/analysis/veloren/details/world_gen_and_civ_deep_analysis.md:3) | 水文、自然條件與文明選址的連接 | 正文較短、部分行號約略；是環境建模線索 |
| [CDDA 三尺度](/home/lorkhan/repo/pas/analysis/cdda/architecture/04_map_three_scales.md:29)、[存檔](/home/lorkhan/repo/pas/analysis/cdda/architecture/09_savegame_serialization.md:85) | 帶尺度與原點的座標型別、近場載入、世界與角色／地圖記憶分開 | 原作尺度與 aetheria 不同，不搬常數與現實泡模型 |
| [DCSS 長期系統](/home/lorkhan/repo/pas/analysis/dcss/architecture/04_long_term_systems.md:13) | 按已過時間與到期點推進腐敗、環境效果等 | 玩家中心地城的計時器，不是遠方文明模擬的完整答案 |
| [opennefia-cpp 摘要](/home/lorkhan/repo/pas/analysis/opennefia-cpp/architecture/summary.md:19) | 核心、事件、原型、保存與 Godot 邊界的既有重寫經驗 | 屬衍生專案完成摘要，只借邊界經驗，不當玩法深度來源 |

PAS 還有地城生成、路徑／群體移動、行為樹與腳本等材料。這次只用與系統藍圖直接相關的正文，不把「庫裡存在」寫成「已全部讀完」。待某個子系統要選算法時，再回該題的資料深入。

## 差距

- 沒有一款參考能直接提供本案要求的三層長期守恆；必須自行寫清時間、配額與主場交接。
- Elin 早期分析、ToME Erebus 草案、LDE3 未解格式與 CK2 部分黑箱，不能直接產生實作規格。
- 類似機制可能採不同政策：整側／單位出手、即時／回合、整圖保留／拆圖重生、固定世界／程序世界。藍圖必須列選項，不能混成單一「原作如此」。
- 上古卷軸、騎砍、文明 5、矮人要塞是使用者指定的整體願景；本輪使用上述材料支援設計，不宣稱完成四作的專項逆向。

## 牽扯到的部份

參考分工進 [三層藍圖](../plan/living-world/roadmap-three-layers.md)，具體系統進 [世界框架](../plan/living-world/world-framework.md) 的下一層文件。先前實作的可重用程度另見 [基線](roadmap-baseline-status.md)。所有參考 repo 保持唯讀；本次產物只寫進 aetheria 文件。
