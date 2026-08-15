# Aetheria

中世紀奇幻背景、日式表現風格的**三層嵌套地圖回合制策略遊戲**。
Godot 4 只負責顯示、美術、音效與 UI；**全部玩法邏輯由 C++ GDExtension 承擔**。

> **現況：規劃階段。** 目前只有設計文件，尚無程式碼、`project.godot` 或建置系統。

## 你來找什麼？

| 你要找的 | 去哪裡 |
|---|---|
| **這遊戲是什麼** | [design/outline.md](design/outline.md) — 三層地圖、尺度、時間、里程碑 |
| **全部設計文件** | [design/README.md](design/README.md) — 索引 |
| **程式架構怎麼分** | [design/tech-stack.md](design/tech-stack.md) |
| **C++ 怎麼寫、用什麼依賴** | [design/cpp-conventions.md](design/cpp-conventions.md) |
| **最難的技術問題在哪** | [design/interface-world-mid.md](design/interface-world-mid.md) 與 [design/interface-lifecycle.md](design/interface-lifecycle.md) |
| **世界怎麼在有限算力下活著** | [design/observer.md](design/observer.md)、[design/significance.md](design/significance.md) |
| **和 medps 是什麼關係** | [design/medps-relation.md](design/medps-relation.md) — **先讀這個再動基礎設施** |
| 動手做某件事（agent） | [AGENTS.md](AGENTS.md) → [wf/WORKFLOWS.md](wf/WORKFLOWS.md) |

## 三層地圖

| 層 | 是什麼 | 網格 | 一格 | 一回合 | 玩法 |
|---|---|---|---|---|---|
| L1 `Region` | 一個大陸或小世界 | 128×96 | 8 km | 一旬（10 天） | 文明系列 |
| L2 `Site` | Region 一格放大 | 64×64 | 125 m | 一小時 | 城建經營 / SRPG |
| L3 `Local` | Site 一格放大 | 64×64 | 約 2 m | 一分鐘 | 開放世界式探索 |

多個 Region 構成一個世界，彼此以 graph 相連（海路、山口、傳送門）。
全部使用 **Square 四鄰接**格。

## 五句話架構

1. **Godot 只畫畫。** 核心 C++ 連 godot-cpp 都不碰，可以 headless 跑完一場遊戲。
2. **Region 是唯一真相**，Site 只是它的一次高解析度展開；數值歸 Region、佈局歸持久層。
3. **兩條 LOD 軸**：observer 決定哪些地方要看清楚，significance 決定哪些實體要個別算。
4. **統計優先，具名例外**：高層事件先對團體算出總量，具名角色再逐一擲判，且必須守恆。
5. **種類都是資料**：地形、河流、道路、建築一律不寫死成 enum，是資料檔裡的 def + 下標。

## 目錄

**非侵入式佈局**：頂層只有 `AGENTS.md`、`CLAUDE.md`、本檔三個 `.md`，
工作流模板的其餘部分收在 `wf/`。

| 路徑 | 內容 |
|---|---|
| `design/` | 全部設計文件。一份一檔、上限 8 KB |
| `wf/` | 工作流 kernel：`WORKFLOWS` / `INDEX` / `DEV-GUIDE` / `SESSION-LOG` / `WAIT_USER` + `workflows/` |

尚未建立（規劃階段之後才會有）：`core/`、`bridge/`、`godot/`、`data/`、`scripts/`、`tests/`。

## 參考專案

| 專案 | 借什麼 |
|---|---|
| **`~/repo/game_dev/medps`** | **同一構想的前一輪**。zone 定址與生命週期、def/Ruleset、EnTT + cereal 序列化、worldgen、奇幻文明 6 roadmap 都已落地或已拍板。見 [design/medps-relation.md](design/medps-relation.md) |
| `~/repo/game_dev/my_godot_assists` | 可複用的 Godot 元件：世界地圖分層、相機、選取高亮、小地圖、角色 |
| `~/repo/game_dev/my-rpg-frontend` | 既有的 Godot + GDExtension 專案佈局（CMake、`.gdextension`） |
| `~/repo/moddings/tome4` | L3 下層地圖的 zone 持久化與生命週期模型 |

## 注意

- 本專案**尚未** `git init`。外層的 `~/repo/game_dev/` 刻意不是 git repo。
- 文件一律繁體中文，單檔上限 8 KB。
