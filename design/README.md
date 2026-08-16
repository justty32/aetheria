# 設計文件索引

**每份文件單一檔案、上限 8 KB**；超過就依子題拆成多個單檔，不要拆成資料夾。繁體中文。

## 先讀（依序）

1. [outline.md](outline.md) — 全局常數：三層命名、尺度、時間、曆法
2. [principles.md](principles.md) — **八條原則。其餘文件都只是它們的展開**
3. [glossary.md](glossary.md) — 術語表。**用詞衝突以此為準**（注意「三層」有三個意思）
4. [medps-relation.md](medps-relation.md) — medps 是同一構想的前一輪，動基礎設施前必讀

## 導讀主線

| 想知道 | 順著讀 |
|---|---|
| 遊戲長什麼樣 | `outline` → `worldmap` → `midmap` → `lowmap` |
| 程式怎麼分 | `tech-stack` → `cpp-conventions` → `zone-model` → `definitions` |
| 最難的地方 | `interface-world-mid` → `interface-lifecycle`（對應 M2、M4） |
| 世界怎麼在有限算力下活著 | `observer` → `significance` → `events` |
| 世界怎麼長出來 | `gen-pipeline` → `edge-consistency` → 各層生成器 |
| 玩起來什麼感覺 | `player-residence` → `combat-scaling` |

## 全部文件

### 大綱

| 文件 | 內容 |
|---|---|
| [outline.md](outline.md) | 定位、三層總表、尺度、時間與可變 stride、曆法、里程碑 |
| [principles.md](principles.md) | 八條貫穿全案的原則 + 設計新機制的檢查表 |
| [glossary.md](glossary.md) | 術語仲裁 |
| [tech-stack.md](tech-stack.md) | `core`/`bridge`/`godot` 三層、跨語言契約、測試、決定論 |
| [cpp-conventions.md](cpp-conventions.md) | C++23、型別與介面約定、vcpkg、建置目標 |
| [build.md](build.md) | M0 實際工具鏈、建置／驗證指令、版本固定與踩坑 |
| [medps-relation.md](medps-relation.md) | 11 條繼承核對清單（已完成）、兩處刻意不同 |

### Zone 與存檔（M0 前置）

| 文件 | 內容 |
|---|---|
| [zone-model.md](zone-model.md) | 一切皆 zone、**定址定案**、root、生命週期 API、三條執行期契約 |
| [zone-save.md](zone-save.md) | 存檔佈局、cereal + EnTT、跨 zone 引用、manifest、fail-fast |

### 三層地圖

| 文件 | 內容 |
|---|---|
| [worldmap.md](worldmap.md) | L1：SoA、三層地形、移動與尋路、世界圖、旬回合 |
| [midmap.md](midmap.md) | L2：Site 種類、小時回合、城建／SRPG／荒野 |
| [lowmap.md](lowmap.md) | L3：人身尺度、垂直層、探索、持久層 |
| [interface-world-mid.md](interface-world-mid.md) | **L1↔L2 界面**：投影／歸約、慢快變數、歸約量表、事件升級 |
| [interface-lifecycle.md](interface-lifecycle.md) | **生命週期**：LOD 狀態機、Digest、重載補算、一致性驗證 |
| [lowmap-streaming.md](lowmap-streaming.md) | 串流；**看可以跨、改必須經中介**；實體搬移 |

### 三條 LOD 軸

| 文件 | 內容 |
|---|---|
| [observer.md](observer.md) | **觀察點**：單根 + 子觀察點樹、四級 LOD、場強與逐出 |
| [significance.md](significance.md) | **重要性**：五級、Cohort、延遲命名、事件也有重要性 |
| [significance-fate.md](significance-fate.md) | **命運判定**：統計優先／具名例外、配額守恆 |
| [unique-objects.md](unique-objects.md) | 獨特物件登記在觀察點上、惰性補算（骨架，未定案） |
| [player-residence.md](player-residence.md) | **玩家駐留層**：省時間 vs 放棄 δ |

### 事件

| 文件 | 內容 |
|---|---|
| [events.md](events.md) | 主場層、跨層面孔、事件信箱、聚合／展開 |
| [event-scaling.md](event-scaling.md) | 升格／降格、**期望值一致**、δ 上界、無偏測試 |
| [combat-scaling.md](combat-scaling.md) | 交戰的三層表現、時間粒度推演 |

### 程序生成

| 文件 | 內容 |
|---|---|
| [gen-pipeline.md](gen-pipeline.md) | 階段即純函數、種子衍生、降維裁決鏈、效能預算 |
| [edge-consistency.md](edge-consistency.md) | **接邊一致性**：規範化 id、邊界剖面、方向陷阱 |
| [worldgen-terrain.md](worldgen-terrain.md) | L1 地形氣候：板塊、雨影、河流、biome |
| [worldgen-civ.md](worldgen-civ.md) | L1 文明：選址、道路、出境點、勢力擴散、歷史層 |
| [sitegen-city.md](sitegen-city.md) | L2 城區：主幹道、街廓遞迴、分區、城牆 |
| [sitegen-wild.md](sitegen-wild.md) | L2 荒野／廢墟／海域 |
| [localgen.md](localgen.md) | L3：三條路線、居住者不預先生成、10 ms |

### 規則（數值待校準）

| 文件 | 內容 |
|---|---|
| [power-tiers.md](power-tiers.md) | **力量體系**：量與質分開、階差門檻與破階手段 |
| [combat-formula.md](combat-formula.md) | **Region 戰鬥公式**：縮放的校準基準 |
| [rules-individual.md](rules-individual.md) | 四屬性、d100、傷害抗性、兵種相剋只在 L2 |
| [rules-magic-faith.md](rules-magic-faith.md) | 魔法／信仰／種族：三種位階來源 + 反面清單 |

### 勢力

| 文件 | 內容 |
|---|---|
| [faction-ai.md](faction-ai.md) | 目標 + 效用評分、性格、**AI 也有 LOD**、情報不對稱 |
| [diplomacy.md](diplomacy.md) | 有向關係四分量、條約、宣戰理由、厭戰值、均勢 |

### 擴展性

| 文件 | 內容 |
|---|---|
| [definitions.md](definitions.md) | **種類一律不寫死 enum**，改資料檔 def + 下標 |
| [rules-extensibility.md](rules-extensibility.md) | 三級擴展：資料表 / Lua / core；腳本的決定論鐵律 |

### 內容與呈現

| 文件 | 內容 |
|---|---|
| [dungeon.md](dungeon.md) | 深度曲線、機關、寶藏神器、`cleared` 防刷、光照壓力 |
| [narrative.md](narrative.md) | 人工只寫骨架，故事靠模擬長出來；三種任務 |
| [art-specs.md](art-specs.md) | 靠組合不靠窮舉、64×64、def id 映射路徑、約 700 張 |
| [art-pipeline.md](art-pipeline.md) | AI 生成四道閘；色盤量化與光源正規化 |
| [audio.md](audio.md) | 三層音景、切層接縫、動態音樂吃既有訊號 |

## 尚未規劃

**設計面的主要項目已全部涵蓋。** 剩下的都是刻意擱置，或本質上屬於實作／內容製作：

| 主題 | 狀態 |
|---|---|
| mark 與獨特物件的細節 | 使用者裁定**先擱置**，等縮放機制穩定後回頭定 |
| root 的成長軸 | 使用者裁定**先擱置**（過早優化） |
| 同層近距離事件的快速路徑 | 已知缺口，刻意留到實作撞到再補（[lowmap-streaming.md](lowmap-streaming.md) 末段） |
| **所有數值** | 各文件的「待細化」幾乎都是數值。要靠**實作 + 校準 + 實測**決定 |
| 內容製作 | 神話、法術清單、建築表、任務庫……屬內容，不屬設計 |

**下一步是實作。** M0 任務書在 `../wf/inbox/m0-bootstrap.md`。

## 寫文件的規約

- 每份文件開頭寫清楚上層是誰、負責回答什麼問題，並指向 `principles.md` 與 `glossary.md`。
- 定死的常數只寫在一個地方（多半是 `outline.md`），其餘引用而不複述。
- 尚未決定的寫進末尾「待細化」，不要假裝已經決定。
- 引用外部專案的結論附**檔案路徑（可附行號）**，不要憑印象。
