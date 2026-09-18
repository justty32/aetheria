# code-map — Rules 與資料

← [code-map](code-map.md)

## `core/rules/` — 不可變規則與純公式

此目錄將 TOML 解析為不可變 Ruleset，並提供移動、個體、戰鬥、力量來源、外交與生成公式。
世界擁有 Ruleset，其他系統借用 const 參考；擁有者析構後 def 指標、span 與執行期下標失效。

`core/rules/ruleset.h` 是公開門面，include `ruleset_definition.h` 與 `ruleset_loader.h`。
`ruleset_storage.inc` 在 Ruleset 類別內文字引入私有儲存宣告，不是獨立 header。
`core/rules/rule_tables.h` 彙整 `rule_tables_*.h` 的地形、文明、Site／Local 規則表，
並保留 enum 定義；其他 `*_rules.h` 按領域命名。

## 檔名群與責任

`ruleset_load_<領域>.cpp` 定義 RulesetLoader 的 private static 載入階段；
`ruleset_load_*.cpp` 的後綴再區分數值、def、引用、城防、風格或機關。
`ruleset*.cpp` 中的無 load 後綴實作承接存取器與總編排。
`ruleset_load_*_detail.h` 共享載入上下文，history detail 只接收入口參考；
`toml_read.h` 是內部 TOML 讀取與驗證 helper，不是對外介面。

載入群涵蓋地形／relief／feature／edge def、biome 第一命中表與季節移動分母、
文明的城市／道路、勢力數／影響力／七項性格與 AI LOD、上古歷史的數值／結構／引用、渡河複合 edge 與完整性驗證、手工世界通道及 canonical 排序、
Site／Local 生成城建、治安／任務門檻、力量／個體／戰鬥／外交／地城資料。
def 型別包含 id／flag 與 TerrainGroundMapping；跨 def 引用與不變式在載入時 fail-fast。
外交資料來自 `data/diplomacy.toml`，涵蓋四分量回歸、條約／理由、厭戰與和談。

`combat*.cpp` 依整數戰役公式、Region 接點、損失與配額分配分工，保留可解釋分解與潰散追擊。
`power_sources*.cpp` 依來源接點、魔法與信仰分工，涵蓋三層魔法、root 信仰與種族上限。
其餘同名前綴的 header／實作提供力量位階、S、個體門檻／破階、四屬性衍生值、單骰 d100 餘量檢定、
資料驅動傷害抗性及建築映射。`*_detail.h` 只供群內實作共享。

## `data/` — TOML 規則來源

此目錄依領域保存 def 與參數，由對應載入群解讀；不是玩法狀態儲存處。
城區骨架、填充與城建資料的責任見 [Site](code-map-site.md)，建築／地下／地城資料見
[Local](code-map-local.md)，外交與 AI 的世界邊界見 [World](code-map-world.md)。
規則載入錯誤、資料擴充、codec 與純公式驗證集中於 [測試詳圖](code-map-tests.md)。
