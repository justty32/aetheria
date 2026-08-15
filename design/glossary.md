# 術語表

> 33 份設計文件是分十幾輪長出來的，同一個詞在不同文件可能指不同東西。
> **這份是仲裁者**：用詞衝突以此為準。
> 2026-08-15 自我審查後建立。

## ⚠ 「三層」有三個意思

這是本專案最容易混淆的一處。**寫文件時一律加限定詞，不要裸用「三層」。**

| 說法 | 指什麼 | 出處 |
|---|---|---|
| **三層地圖** | L1 Region / L2 Site / L3 Local | [outline.md](outline.md) |
| **三層資料** | 程序層 / 持久層 / 易失層 | [principles.md](principles.md) 原則三 |
| **三層程式碼架構** | `core/` / `bridge/` / `godot/` | [tech-stack.md](tech-stack.md) |

「完整三層資料」≠「完整三層地圖」。看到裸的「三層」要回頭確認它指哪個。

## 地圖層級

| 代號 | 程式碼名 | 網格 | 一格 |
|---|---|---|---|
| L0 | `WorldGraph` | 圖（非格子） | 一個 Region |
| L1 | `Region` | 128×96 | 8 km |
| L2 | `Site` | 64×64 | 125 m |
| L3 | `Local` | 64×64 | 約 2 m |

**不使用**「大／中／小地圖」以外的別名。中文行文可以說「大地圖／中層／下層」，
但程式碼一律用 `Region`／`Site`／`Local`。

## 座標與識別

| 型別 | 是什麼 | 範圍 |
|---|---|---|
| `ZoneKey` | **zone 的位址**。座標推導，見 [zone-model.md](zone-model.md) | uint64 |
| `RegionXY` | Region 內的格位 | 128×96 |
| `SiteXY` | Site 內的格位 | 64×64 |
| `LocalXY` | Local 內的格位 | 64×64 |
| `EntityId` | **zone 內**的實體。實質是 `entt::entity`，強型別包裝 | per-registry |
| `EntityRef` | **跨 zone** 的弱引用 `{ZoneKey, uid}`，解引用可失敗 | 全域 |
| `FactionId` | 勢力 | — |
| `*DefId`（`TerrainId`、`EdgeId`…） | **資料檔定義的下標**，枚舉子一個都不列 | 見 [definitions.md](definitions.md) |

**沒有 `SiteId` 這個型別。** 早期文件用過，已作廢——
Site 的身分就是它的 `ZoneKey`，由 Region 座標推導而得，不另外配發。

`EntityId` 只在自己的 registry 內有意義。**跨 zone 一律用 `EntityRef`**，
絕不存裸 `entt::entity`（搬移之後立刻失效）。

## LOD 等級

四級，**三層地圖共用同一組名稱**（見 [observer.md](observer.md)）：

| 等級 | 意思 |
|---|---|
| `L_FULL` | 完整載入，逐回合模擬，個別實體都算 |
| `L_COARSE` | 骨架 + 持久層，只跑聚合統計 |
| `L_FROZEN` | 只有 digest 常駐，不跑，查詢時即時投影 |
| `L_ABSENT` | 不在記憶體 |

**注意**：`L_FULL` 的「完整」指的是**三層資料**都在，不是「三層地圖」。

## Digest

| 名稱 | 是什麼 |
|---|---|
| `SiteDigest` / `LocalDigest` | zone 卸載時壓縮成的持久層摘要。**同一個機制的兩個實例**，欄位相同、尺寸不同 |
| `StatusDigest` | 獨特物件的簡要狀態快照，見 [unique-objects.md](unique-objects.md) |

前兩者實作上應該是同一個模板，`Site`／`Local` 只是名字。
若實作時發現欄位真的分歧，回報給規劃者更新設計。

## 時間

| 詞 | 意思 |
|---|---|
| `Tick` | 全局時鐘，**單位為秒**，`int64_t` |
| **stride** | 某層某狀態下「一回合等於幾秒」。**可變**（交戰時縮短） |
| **旬** | 10 天 = 864,000 秒。L1 的固定 stride |

⚠ **不要把「回合數」當時間單位。** stride 可變，
「一旬 = 240 個 Site 回合」只在**平時**成立，交戰時是 960 個。
需要時間就用 `Tick`，需要回合就明說是哪一層哪個狀態的回合。

## 變數的快與慢

見 [interface-world-mid.md](interface-world-mid.md)：

| | 慢變數 | 快變數 |
|---|---|---|
| 例 | 地形、河流、道路、seed | 人口、建設等級、owner、駐軍 |
| 決定 | **骨架** | 填充與狀態 |

**骨架只准依賴慢變數**（[principles.md](principles.md) 原則四）。

## 三條 LOD 軸

| 軸 | 決定什麼 | 文件 |
|---|---|---|
| **Observer**（空間） | 哪些**地方**要載入、多細 | [observer.md](observer.md) |
| **Significance**（實體） | 哪些**實體**要個別算、要不要有名字 | [significance.md](significance.md) |
| **Event scaling**（事件） | 哪些**事情**要展開、在哪一層推進 | [events.md](events.md) |

早期文件可能寫「兩條軸」，那是事件縮放引入前的舊說法，以本表為準。

## Observer 相關

| 詞 | 意思 |
|---|---|
| **根 observer** | **只有一個：玩家** |
| **子觀察點** | 從根分出去、掛在事件／物件／地點上的。可再分孫 |
| **場強 score** | `max over observers of (strength − travel_cost)`，決定 LOD |
| **駐留層** | **玩家自己**選擇站在哪一層操作，見 [player-residence.md](player-residence.md)。與場強正交 |
| `pinned` | 永不降級、不受預算約束 |

早期文件可能把 observer 寫成「多個平行來源」，那是修正前的舊說法——
**正解是單根 + 樹**。

## 力量與重要性

`significance` 與 `power tier` **共用同一組五級名稱**，但**升級規則不同**：

| | 提升方式 |
|---|---|
| **significance**（被個別計算的資格） | 可因**聚合**提升——一千個流民聚成流民團就升級 |
| **power tier**（戰鬥位階） | **不因人多提升**——十個騎士組隊，位階仍是 2 |

這是目前最容易寫混的一處，見 [power-tiers.md](power-tiers.md) 末段。

## 待補

新詞進來時加進這裡。發現兩份文件用詞衝突而本表沒收錄，
先回報給規劃者，不要自行挑一邊。
