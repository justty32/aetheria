# code-map — Site

← [code-map](code-map.md)

## `core/site/` — L1→L2 界面與城區／荒野

本目錄負責慢／快變數投影、64×64 骨架、填充、戰鬥、城建、駐留與 Site→Region 歸約，
全部為純 C++。細部入口見 [Site README](../../../core/site/README.md)。
`core/site/site_projection.h` 是門面，保留 enum，彙整 `site_projection_vars.h`
與 `site_projection_layers.h` 的變數、程序／持久／揮發層及投影介面。
持久資料包括治安、具名 NPC 與地城。

`site_<職責>_<階段>.cpp` 依職責分檔，`*_detail.h` 共享該群內部介面。
同前綴不代表可互相回寫所有資料：骨架、程序填充、持久來源與歸約仍有各自的寫入邊界。

## 城區生成與資料

`site_skeleton_*.cpp` 承接 S1～S4：高度內插、低頻噪聲及邊界水體；固定兩趟水源距離與
城心評分；道路 edge 接城門、最多四條 A* 主幹道與路段重用；遞迴街廓及水／路／坡度可建地遮罩。
投影入口負責 Region tile 拆分、seed 與骨架完整 hash。

填充群以 population、zoning、building、fortification、landmark、damage 前綴辨識，
負責 F1～F5 編排與驗證：依人口／建設等級配額及街廓偏好分區、deterministic 配對、
依 def 與臨街條件貪婪 packing、城牆／城門／雙重牆／塔樓／護城河、owner 風格地標，
以及依城門／缺口距離施加瓦礫／焚毀。填充**不准回寫骨架**，並驗有牆必有門及填充 hash。

`data/site_projection.toml` 保存 Terrain→Ground 映射與骨架規則；`data/site_city.toml`
保存配額、建築、城防引用、勢力風格與缺口比例。對應 SiteGenerationRules 與 SiteFillRules；
啟用分區缺配額或建築 def 時載入失敗。`data/site_build.toml` 保存建造時數、產出、容量、
相鄰效果及人口成長參數。載入群歸 [Rules](code-map-rules.md)。

## 荒野 W1～W6

`site_wilderness*.cpp` 依 boundary、terrain、paths、content、entities 等後綴分工：
共用 canonical edge／corner、角錨定、Region 慢變數剖面，四邊內插與 relief 噪聲，
海域通行遮罩、有界 A* 河湖／道路／橋、抖動植被、廢墟街廓複用及 L3 入口。
W5 是唯一快變數入口；完整荒野 hash 與 live registry 實體安裝也在此群。
荒野 live component 不進存檔白名單，冷載重算程序內容。
`data/site_wild.toml` 管坡度、密度、入口及廢墟保留比例。

## 駐留與時間

`site_materialize*.cpp` 分首次展開、冷重算、digest 恢復、Frozen 凍結／解凍與逐出收回，
依聚落有無分流城區／荒野。`site_streaming*.cpp` 透過 ZoneManager 單一取得入口升載，
玩家跨格重算 3×3 FULL／5×5 COARSE 場，尾端升降級、延遲逐出並批次推進 FULL Site。

`site_lifecycle*.cpp` 分 digest、閉式補算／飽和與骨架失效後的位置搜尋及遷移，
保留／就近搬移／災變毀壞會保留持久狀態並產生敘事事件。
時鐘式 `unload_site_zone`／重載補算與舊 `collapse_site_zone` 的無時間往返語意不同，
不能混用。

## 城建、戰鬥與歸約

`site_build_loop*.cpp`、`site_build_economy*.cpp` 分工地命令、狀態驗證、每小時經濟及批次旬界，
負責 ZoneKey 正規化、持久 CityBuildState、建造進度、糧食／生產、相鄰效果、人口成長與 Region 回填。
L_FULL 批次共享全局時鐘，旬界只結算 Region 一次。

`site_combat*.cpp` 分回合、攻擊與 cohort 部署／機動，以 15 分鐘為步長，處理陣型／面向、
地形／command 與四組條件式兵種相剋；戰術壓力調整 Region 傷亡配額。
`site_reduction*.cpp` 分觀測與 Region 回填，固定 row，治安 optional 保留缺席與觀測到 0 的差別。
Local→Site 四列私有快變數 storage 也在此目錄；兩層 ReductionTable 各為唯一 writer。
`site_event_escalation.*` 先將事件寫入持久來源，達 Region 級才立即同步歸約快變數。
共用 typed storage 與絕對快照套用歸 [Spatial](code-map-local.md)。

## `tests/site/` — 證據導航

測試採 `<主題>_<子題>_test.cpp` 與 `*_test_support.h`，依前綴找群組。
骨架／填充群驗 S1～S4、快慢隔離、有牆必有門／雙重牆、地標、損毀偏好、packing 與 S+F 效能；
materialize／roundtrip 群以真遮罩驗持久座標、三次冷往返與非空程序層重算。

build 群驗人口演化、批次旬界、輸入順序無關、單 Site 等價、兩種擺法、絕對歸約不重複及 pending 冷載；
observation persistence 群驗 v16 Site 拒讀、觀測欄位逐項進世界 hash 與 v18 冷往返。
combat 群驗 3 格 stride、四組相剋成立／不成立、地形／command、決定性、事件歸約與單場效能。

unload equivalence 群以 N=1／5／20／100 旬驗 FULL／ABSENT 四列誤差、持久物件集合、負向控制與
24 旬老化上限，另固定 20 旬量測 0／2／5／10／20 次卸載偏差。
migration 群人工改慢變數後冷載，驗三種結局、無非法 live 座標、毀壞／事件持久化，及僅搬移仍有事件。
wilderness 群驗共享邊五項判準、W1～W6、海域／廢墟、零玩法持久物與冷重算，並守 Debug／Release 30 ms 預算。
