# design/ 索引 — 全部設計文件

> 初次進來請先讀 [README.md](README.md)（先讀順序、導讀主線、寫文件的規約）。
> 本檔只做一件事：**列出 `design/` 的全部文件與各自負責什麼**。


### 大綱

| 文件 | 內容 |
|---|---|
| [outline.md](outline.md) | 定位、三層總表、尺度、時間與可變 stride、曆法、里程碑 |
| [principles.md](principles.md) | 八條貫穿全案的原則 + 設計新機制的檢查表 |
| [glossary.md](glossary.md) | 術語仲裁 |
| [tech-stack.md](tech-stack.md) | `core`/`bridge`/`godot` 三層、跨語言契約、測試、決定論 |
| [cpp-conventions.md](cpp-conventions.md) | C++23、型別與介面約定、vcpkg、建置目標 |
| [time-model.md](time-model.md) | **時刻 vs 時距**、合法運算表、合法域與 `AETH_CHECK`、曆法精度界線 |
| [time-clock-authority.md](time-clock-authority.md) | **誰有資格說「現在幾點」**；時距／時戳／時鐘讀數的三分 |
| [build.md](build.md) | M0 實際工具鏈、建置／驗證指令、版本固定與踩坑 |
| [medps-relation.md](medps-relation.md) | 與 medps 的關係、取向裁定、繼承核對清單、兩處刻意不同 |
| [medps-inheritance.md](medps-inheritance.md) | 逐條比對細節：已拍板繼承的決策表、刻意不同的比對表 |

### Zone 與存檔（M0 前置）

| 文件 | 內容 |
|---|---|
| [zone-model.md](zone-model.md) | 一切皆 zone、root、生命週期 API、成長軸不變量 |
| [zone-contracts.md](zone-contracts.md) | 執行期契約：tick 內禁止結構變更、作用域借用、命令緩衝 |
| [zone-addressing.md](zone-addressing.md) | **定址定案**：`ZoneKey` 位元佈局、座標推導、Detached |
| [zone-save.md](zone-save.md) | 存檔目錄佈局、路徑由 key 推導、manifest、開檔協定、fail-fast |
| [zone-save-format.md](zone-save-format.md) | 存檔位元流：cereal + EnTT snapshot、`AllComponents`、版本沿革 |

### 三層地圖

| 文件 | 內容 |
|---|---|
| [worldmap.md](worldmap.md) | L1：SoA、三層地形、移動與尋路、世界圖、旬回合 |
| [midmap.md](midmap.md) | L2：Site 種類、小時回合、城建／SRPG／荒野 |
| [lowmap.md](lowmap.md) | L3：人身尺度、垂直層、探索、持久層 |
| [interface-world-mid.md](interface-world-mid.md) | **L1↔L2 界面**：投影／歸約、慢快變數、歸約量表、事件升級 |
| [interface-lifecycle.md](interface-lifecycle.md) | **生命週期**：LOD 狀態機、Digest、重載補算、骨架失效 |
| [interface-verification.md](interface-verification.md) | **界面驗證**：M4 判準、負向控制、假通過陷阱 |
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
| [worldgen-terrain.md](worldgen-terrain.md) | L1 地形：板塊、高度、河流、biome |
| [worldgen-climate.md](worldgen-climate.md) | L1 氣候：溫度、盛行風、降水與雨影 |
| [worldgen-civ.md](worldgen-civ.md) | L1 文明：選址、道路、出境點 |
| [worldgen-history.md](worldgen-history.md) | L1 歷史層：上古選址、古道、廢墟 |
| [worldgen-factions.md](worldgen-factions.md) | L1 勢力：首都採樣、影響力擴散、國界 |
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
| [dungeon.md](dungeon.md) | 深度曲線、機關、寶藏、`cleared` 防刷、光照壓力 |
| [narrative.md](narrative.md) | 人工只寫骨架，故事靠模擬長出來；三種任務 |
| [art-specs.md](art-specs.md) | 靠組合不靠窮舉、64×64、def id 映射路徑、約 700 張 |
| [art-pipeline.md](art-pipeline.md) | AI 生成四道閘；色盤量化與光源正規化 |
| [audio.md](audio.md) | 三層音景、切層接縫、動態音樂吃既有訊號 |
