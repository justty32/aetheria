# 舊需求、新系統與參考對照

[本層入口](README.md)｜← [規劃總覽](roadmap-2026-09.md)｜[世界框架](world-framework.md)｜[現況基線](../../investigation/roadmap-baseline-status.md)

## 原有需求不能在重排時消失

這張表追蹤需求去向，不表示目前已完成。未來的 R 輪次見總覽，M 編號保留作歷史追溯。

18 條原有需求的去向（原有需求／新規劃歸屬／必須保留的驗收）已抽到 [roadmap-coverage.json](roadmap-coverage.json)。

M10 的原始範圍以 [既有總覽](../runtime-injection.md) 與 [裁定](../runtime-injection-decisions.md) 為準。版本號由實際整合決定，不因為重排就先改存檔格式。整個 M10 是一份完整承諾，先做一類內容不等於完成。

## 使用者指定參考的分工

| 參考 | 本次吸收的位置 | 來源盤點 |
|---|---|---|
| Endless Legend | 大地圖系統與架構、經濟、外交、大局 AI | [世界參考](../../investigation/roadmap-reference-overview-status.md) |
| VCMI、Wesnoth | 輔助軍團展開、地形查詢與戰果回寫；中層主定位已回歸原案 | [中層](../../investigation/roadmap-reference-tactics-status.md) |
| Total War（原案定位） | 方陣機動、戰線與側背，保留回合制；不是引擎移植 | [既有交戰設計](../../../../design/simulation/combat-scaling.md) |
| ToME4、RimWorld、Elin | 小層探索、技能物品、生活工作、人物互動 | [小層](../../investigation/roadmap-reference-local-status.md) |
| guigu、michangsheng、xiuxian | 人物目標、關係、成長與遠方模擬 | [人物補充](../../investigation/roadmap-reference-people-status.md) |
| longyin、sanguo | 人物與勢力派工、共同時序、組織關係 | [社會補充](../../investigation/roadmap-reference-society-status.md) |
| CK2、taikou、lde3 | 職位／繼承／法理、人生職涯、任務鏈 | [PAS 社會參考](../../investigation/roadmap-reference-pas-status.md) |

實際資料夾名為 `xiuxian`、`taikou`、`crusader-kings-2`。前者對應使用者所寫 `xuixian`；太閤對應所寫 `taiko`。本輪共十四個指定參考，另讀六項 PAS 補充，範圍見世界參考報告。

## 願景如何落到系統

| 使用者要的世界感 | 規劃中的具體支撐 |
|---|---|
| 能走進去生活的奇幻世界 | 人物、生活、旅行、力量、呈現；場所能互動、人物有經歷、規則有代價 |
| 能帶隊介入大勢 | 大局 AI、戰爭、政治、三層駐留；上層軍隊就是下層的參與者 |
| 文明會經營、競爭與興衰 | 經濟、政治、知識、地理、生態、外交；每條發展都有資源與制度條件 |
| 細節能自然產生故事 | 生活、人物、歷史；工作、傷病、關係、地方改動形成長期因果 |
| 世界可持續加入新內容 | 內容、模擬契約；文明、人物、物品、地方、劇情進同一歷史 |
| 粗到細都保持同一世界 | 時間、身分、配額與事件主場；每個系統都明列三層分工 |

## 不直接採納的原作做法

不是刪掉玩法，而是避免移植時破壞本案的三層結構：不把 Endless 的省份當 Region；不直接搬 VCMI 七槽或 Wesnoth 六角；不讓 RimWorld 的全圖即時 tick 取代回合；不因 Elin 早期分析寫了某 API 就當它已複驗；不拿 LDE3 或 ToME Erebus 的推測當現成架構；不讓 CK2 的頭銜模型取消本案獨立勢力身分。

可改的政策保留方案與理由。例如中層出手順序、繼承方式、工作預約、遠方精度、法術副作用都能大膽設計；已定的三層邊界與存檔承諾不能靠換詞省掉。
