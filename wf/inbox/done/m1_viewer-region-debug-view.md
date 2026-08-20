# 信：任務書 — Region 除錯檢視器（讓生成結果第一次被看見）

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**回信地址**：`~/repo/game_dev/aetheria-viewer/wf/inbox/`
**必讀設計**：[`design/worldmap.md`](../../design/worldmap.md) 的「三層地形」、
[`AGENTS.md`](../../AGENTS.md) 的架構鐵律
**基準**：`6b03571`（分支 `m1-viewer`，獨立 worktree）

---

## 為什麼做這個

十二個生成階段快做完了，但**看它的唯一方法是灰階 PGM**。

於是每一個「這樣好不好」的問題——廢墟落在哪、道路密不密、國界什麼形狀、biome 對不對——
都只能在黑暗裡爭論數字。已經因此浪費過兩輪了。**這個工具的目的就是止血。**

## 你在一個獨立的 worktree 裡

`~/repo/game_dev/aetheria-viewer`，分支 `m1-viewer`。
**另一條線**（另一個你）正在主 worktree `~/repo/game_dev/aetheria` 做 M1.6，
會改 `core/`、`data/`、存檔格式。硬約束：

> **你只碰 `godot/` 與 `bridge/`，外加 `cmake/targets_bridge.cmake` 若真的需要。**
> **不准改 `core/`、`data/`、`sim/`、`tests/` 任何檔案。**
> 也不准碰主 worktree。

如果你發現非改 `core/` 不可才做得下去——**停下來寫信**，不要硬幹。

## 這是除錯工具，不是遊戲的地圖渲染器

**明確裁定，免得你往錯的方向做**：`worldmap.md` 寫的三層 `TileMapLayer` 是**將來有美術之後**
的正式渲染路徑。現在沒有任何美術素材，去搭 TileSet 只會做出一堆假進度。

所以這輪：**用最簡單、最不會壞的方式把像素畫出來就好**——
例如產生一張 `Image` / `ImageTexture`（每格 N×N 像素，N 取 8 左右，
好讓「邊」畫得進格子之間），放大顯示。怎麼實作你決定，**理由寫進回信**。

## 架構鐵律（這條不能破）

`AGENTS.md`：**Godot 端不得持有玩法狀態，只做顯示／美術／音效／UI／輸入轉發。**

所以 bridge 只提供**唯讀視圖**：給定 seed 與 region_id，呼叫既有的
`build_skeleton()` + `populate()`，把 `RegionTiles` 的各欄位以 Packed\*Array 傳給 Godot。
Godot 端只拿去畫，不保存、不修改、不推進任何狀態。

**只依賴 `populate()` 回傳的 `RegionTiles` 既有公開欄位**
（`base`／`relief`／`feature`／`temperature`／`moisture`／`elevation`／`edges`／`settlement`），
不要依賴 M1.6 正在加的東西（`owner` 的填充、portal）——那條線還在跑，等它落地再接。

## 範圍：一張看得到的圖 + 圖層開關

- 輸入 seed 與 region_id，按一個鍵重新生成。
- 可切換的圖層（鍵盤 1～7 之類，畫面上要有說明文字）：
  1. **基底地形**（terrain 各給一個好認的顏色）
  2. **起伏**（relief 疊加明暗）
  3. **高度**（heatmap）
  4. **溫度**
  5. **濕度**
  6. **地物**（森林／礦脈／綠洲／地標／三級廢墟，各給好認的標記）
  7. **邊**（河流三級用藍色深淺、道路三級用棕色深淺、**古道另給一個顏色**、橋／渡口再一個）
  8. **聚落**（大城／城鎮／村莊三種大小）
- 顏色**不必好看，必須好認**。這是儀器不是美術。
- 顏色表寫在**一個集中的地方**（GDScript 常數表或一個小資料檔），不要散在畫圖迴圈裡。

## Done when

- [ ] `godot --headless` 跑得起來、exit 0（沿用既有 headless 驗證方式）
- [ ] **能匯出一張 PNG**（headless 也能匯出），這樣沒開 GUI 的人也看得到結果
- [ ] 附上**至少 3 個不同 seed 的 PNG** 放在 worktree 的 `out/`（`.gitignore` 已擋，不進版控），
      並在回信描述你在圖上**實際看到了什麼**（大陸形狀、河流有沒有從高處流到海、
      道路有沒有連起來、城市是不是擠在一起、廢墟散布如何）
- [ ] 八個圖層都能切換且都畫得出東西
- [ ] `aetheria_core` 仍零 godot-cpp（`CoreIsolation.CompileCommands` 通過）
- [ ] **`core/`、`data/`、`sim/`、`tests/` 零改動**（`git diff --stat` 貼出來自證）
- [ ] 四 target 零警告、CTest 全綠
- [ ] commit 到 `m1-viewer` 分支（**不 push、不 merge 回 main**）

## 不要做的事

| 不做 | 為什麼 |
|---|---|
| 搭正式的三層 TileMapLayer + TileSet | 沒有美術，會做出假進度。有美術了再說 |
| 改 `core/`／`data/`／`sim/`／`tests/` | 另一條線正在改，會撞。真的需要就停下寫信 |
| 畫 `owner`（勢力）與 portal | M1.6 還在做，資料還沒有。**等它落地** |
| 在 Godot 端存任何玩法狀態 | 架構鐵律 |
| 調生成參數把地圖弄好看 | 你的工作是讓它**被看見**，不是讓它好看 |
| **為了自我審查 fan-out 一堆子 agent** | 驗收我自己會跑 |

## 回信給我

寫成 `wf/inbox/m1-viewer-complete.md`（在這個 worktree 裡）。三個問題：

1. **你在圖上實際看到了什麼？** 這是本任務真正的產出。地形合不合理？
   河流有沒有從高處流向海？道路連不連得起來？城市擠不擠？**看到什麼就說什麼，
   包括難看的地方**——我要的就是這個。
2. **有沒有哪一層畫出來讓你覺得「這看起來壞掉了」？** 貼具體現象。
   這比任何數值驗收都有價值。
3. **實作方式你選了什麼**（ImageTexture／其他），為什麼？
