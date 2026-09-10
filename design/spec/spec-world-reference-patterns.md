# 從 gameplots 借哪些世界變化的想法

← [本層入口](README.md)｜[背景](spec-world-background.md)｜[規劃尺度](spec-planning-scope.md)

## 來源與用途

2026-09-10 使用者指示參考 gameplots。本機實際位置是 `/mnt/c/code/mine/gameplots`；本輪唯讀該庫的整理結果，沒有修改它，也沒有追加網路研究。下列相對路徑都以該目錄為根。

這是創作參考庫，內容包含整理者對作品的解釋。本輪只借其中的關係與衝突模式，不把它當成已核實的官方設定或可直接實作的 AI 規則。具體原作人物、數字、結局和宇宙觀都不作本案前提。

**Done when：**讀過的模式有明確來源，能指出本案已有什麼、補了什麼；真正新增的行為回到所屬正文，不讓參考筆記另管世界規則。DQ 暫定背景、NPC 自主與玩家體驗後置維持。

## 本輪真正補進去的三件事

| 整理庫提供的啟發 | aetheria 怎麼用 | 正文 |
|---|---|---|
| 《鈴蘭之劍》的伊利亞整理與《永恆之柱》的商業公司整理：重要資源牽動本地與外部利益 | 小國可以有議價能力，選限量供應、分散買家、交換護送；國內不同人也能反對獨家安排。合作可互利，不預定小國必被犧牲 | [國家差異](spec-country-patterns.md)「小國也可能握有別人需要的東西」 |
| RuneScape 的神際限制、《Fall from Heaven》的盟約整理：強者也受承諾、調停與限制影響 | 調停者或約束條件改變後，各神按所知重議、遵守、試探或退出。政治約定與真正阻擋能力的作用分開；不自動全體介入 | [神明自主](spec-deity-behavior.md)「合作、競爭與失敗」 |
| 《Guild Wars 2》的巨龍與循環整理：危險存在也可能承擔某項作用 | 若本局設定確有依賴，NPC 可查證、先找替代、交涉或避開；直接解除也可能留下後果。不能每次勝利才臨時發明新災難 | [時代變革](common/spec-world-eras.md)「同一起點可以走向不同世界」 |

這三件事都只是候選內容的通用接法。沒有新增固定資源戰爭、統一神際盟約或全世界魔力平衡系統。

## 已經有的，就不再重寫

- 《永恆之柱》的海盜聯合與《鈴蘭之劍》的傭兵團整理，提示組織不靠領土也能有力量，內部還能對去向有分歧。[中小勢力](spec-minor-powers.md) 已有通路、本事、契約、多重隸屬、分合與退出；不再添一份組織規則。
- Eothas 的整理提示長期目的和當前手段可以分開，改用新手段仍留下舊代價。神明篇與共同 AI 已有改案及失敗記憶，無須照搬原作的指定行動或結局。
- 《Fall from Heaven》的時代與末日整理，適合提醒「各方能推動或阻止變化」。本案已按本局來源、局部行動與後果接續，不採固定末日計數器、必經紀元或預定拯救者。

## 實讀來源位置

先讀 `readme.md` 與 `for_agent.md`，以 `results/` 的作品索引導向正文。以下列本輪取用的主要位置，便於回查；本案執行不依賴這個外部目錄存在。

| 題目 | gameplots 內相對路徑 |
|---|---|
| 小國與資源 | `results/鈴蘭之劍/factions/Kingdom_of_Iria.md` |
| 商業組織與國家利益 | `results/Pillars_of_Eternity/factions/vailian_trading_company.md` |
| 無領土組織與內部分歧 | `results/Pillars_of_Eternity/factions/principi.md`；`results/鈴蘭之劍/factions/Sword_of_Convallaria.md` |
| 神際限制與變動 | `results/RuneScape/concepts.md`、`results/RuneScape/events.md`；`results/Fall_from_Heaven/concepts.md` 的盟約段 |
| 神自身的目的與手段 | `results/Pillars_of_Eternity/characters/Eothas.md`；`results/Pillars_of_Eternity/concepts/the_made_gods.md` |
| 危機與維持作用 | `results/Guild_Wars_2/concepts.md`、`results/Guild_Wars_2/events.md` |
| 紀元與災變 | `results/Fall_from_Heaven/concepts.md` 的五大時代、末日段 |

來源是閱讀當時的本機工作樹；本輪沒有逐條回查原作。若日後需要正式陳述某作品的設定或機制，再另行核實，不能把這份轉化筆記當成原作考據。
