# 信：任務書 M1.6-prep — 影響力擴散純函式（**不接管線**）

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**回信地址**：`~/repo/game_dev/aetheria-influence/wf/inbox/`
**必讀設計**：[`design/worldgen-civ.md`](../../design/worldgen-civ.md) 第 12 節、
[`design/gen-pipeline.md`](../../design/gen-pipeline.md) 的「決定論的驗證」
**基準**：`3649a8d`（分支 `m16-influence`）

---

## 你在一個獨立的 worktree 裡

`~/repo/game_dev/aetheria-influence`，分支 `m16-influence`。
**另一條線**（另一個你）正在主 worktree 做 M1.5.1，改的是 `data/civilization.toml`
與上古選址權重。所以本任務有一條硬約束：

> **不准動 `data/` 底下任何檔案。** 參數這輪先用一個 C++ config struct，
> 等 M1.6 正式接管線時再搬進 `civilization.toml` 的 `[factions]`。
> 這是為了避開與 M1.5.1 撞同一個資料檔，不是我改變了「數值住資料檔」的鐵律。

同理**不准動** `core/worldgen/region_build.cpp`、`region_config.h` 的參數 group、
`gen_stage_ids.h`、存檔格式、`core/worldgen/city_*`、`core/worldgen/history_*`。

你這輪只新增檔案，加上 `cmake/targets_core.cmake` 與 `cmake/targets_tests.cmake` 各一行。

## 為什麼要先做這個

`worldgen-civ.md` 第 12 節的影響力擴散是**整條生成管線第二個順序相依的演算法**
（第一個是道路重用折扣，M1.4 已處理）。多源洪水填充如果寫成「每個勢力依序 flood fill」，
勢力的處理順序就會決定邊界長在哪——而且它**不會報錯，只會安靜地給出不同的國界**。

所以我要在它被接進管線、被存檔、被上面疊玩法之前，先把它單獨做對、單獨證明。

## 範圍：兩個純函式 + 它們的測試

### 1. `select_capitals()` — 最遠點採樣

設計原話：「每個勢力分到一個大城作為首都，首都之間用**最遠點採樣**盡量分散」。

- 輸入：城市清單（`CitySite`）、勢力數
- 只有 `SettlementTier::City` 有資格當首都
- 第一個首都的選法要**顯式定死**（例如分數最高、平手用 canonical id），不要隨便挑
- 之後每次選「離已選首都集合最遠的那一座」，距離用什麼**你決定但要寫清楚理由**
  （曼哈頓或實際移動成本都可以，講清楚為什麼）
- 勢力數超過可用大城數 → **fail-fast，throw**，不要默默少給

### 2. `spread_influence()` — 多源擴散

設計原話：「從首都做影響力擴散（洪水填充，成本用移動成本，遇山海衰減），
把周圍的城鎮村莊與 tile 的 `owner` 分配出去。影響力相遇處形成邊界。」

- **必須是多源 Dijkstra，不是逐勢力依序 flood fill。** 所有首都同時進 priority queue。
- **平手要顯式定死**：同一格被兩個勢力以**相同**影響力成本抵達時，
  由 `(成本, 勢力 canonical 序, 首都 tile 線性下標)` 決勝——**絕不靠插入順序或容器順序**。
- 成本用移動成本（複用 `core/world/region_movement.h` 那套，不要另寫一份）。
- **影響力預算耗盡就停**，走不到的格子 `owner = FactionId{0}`（無主）。
- **海格不擴散**：`worldmap.md` 說海格只有船能進，影響力止於海岸。
- 輸出是一個 `owner` 向量（`std::vector<world::FactionId>`），不寫進 `RegionTiles`。

## Done when

- [ ] **打亂首都輸入順序 → `owner` 向量逐位元相同**（貼兩組雜湊）
- [ ] **負向控制**：故意改成依輸入順序逐一 flood fill，證明輸出**確實會變**
      （貼兩組不同的雜湊）。這條沒抓到就代表上一條是假的
- [ ] **平手真的會發生**：貼「有多少格是被兩個以上勢力以相同成本抵達的」。
      如果是 0，代表你的測試地形太乾淨，**換一個會產生平手的合成探針**——
      平手不發生的話，tie-break 規則等於沒被測到
- [ ] **邊界沿地形長**：貼一個證據說明國界確實被山脈／河流推著走
      （例如：邊界格的平均移動成本 vs 全圖平均，應該顯著較高）
- [ ] **無主區存在**：貼無主格數量與佔比。若是 0，把影響力預算調小到會出現無主區為止
- [ ] `select_capitals()` 的決定論：打亂輸入城市順序 → 選出的首都集合相同
- [ ] 勢力數 > 可用大城數 → throw（負向測試）
- [ ] 兩個函式都是**純函式**：不碰全域、不讀時鐘、不碰檔案系統
- [ ] `aetheria_tests` 零警告、CTest 全綠、`aetheria_core` 仍零 godot-cpp
- [ ] commit 到 `m16-influence` 分支（**不 push、不 merge 回 main**）

## 不要做的事

| 不做 | 為什麼 |
|---|---|
| 接進生成管線 | M1.6 正式那輪才接，我要先看到它單獨是對的 |
| 動 `data/` 任何檔案 | 另一條線正在改，會撞 |
| 動存檔格式、參數 group、stage id | 都不在這輪 |
| 出境點（階段 11） | 另一份任務 |
| 勢力的資料表（名稱、外交、資源） | 這輪只做 `FactionId` 分配 |
| 回頭替 M0～M1.5 已通過的功能補驗證 | 已驗收完畢 |
| **為了自我審查 fan-out 一堆子 agent** | 驗收我自己會跑。你把數字量準就好 |

## 回信給我

寫成 `wf/inbox/m1-6-prep-influence-complete.md`（在這個 worktree 裡）。兩個問題：

1. **平手到底發生了幾次？** 這決定 tie-break 規則有沒有真的被測到。
2. **最遠點採樣你用了哪種距離，為什麼？** 曼哈頓便宜但會讓隔著海灣的兩個首都看起來很遠；
   實際移動成本貴但誠實。你的選擇與理由寫進來，我裁定。
