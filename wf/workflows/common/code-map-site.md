# code-map — `core/site`

← [總 code map](code-map.md)｜[conventions](conventions.md)

`core/site/` 是 L1→L2 的界面與城區生成邊界，全部為純 C++，不得依賴 godot-cpp。

`core/spatial/boundary_profile.h` 是兩層共用的 canonical edge/corner、角錨定與剖面生成；
`core/local/local_tiles.h` + `local_{projection,generation,materialize}.cpp` 是 L2→L3 的單層
`LocalTiles`、路線 B、正規化雜湊，以及彼此分離的首次展開／純冷載／冷重算入口。

| 檔案 | 職責 |
|---|---|
| `site_projection.h` | 慢／快變數隔離、64×64 骨架與三層資料的公開型別／入口 |
| `site_projection.cpp` | Region tile 拆分、seed、S1～S4 編排與骨架完整雜湊 |
| `site_skeleton_detail.h` | 骨架分段實作的內部共用介面 |
| `site_skeleton_common.cpp` | 邊界 crossing 位置與局部坡度 |
| `site_skeleton_terrain.cpp` | S1：高度內插、低頻噪聲、邊界水體延伸 |
| `site_skeleton_center.cpp` | S2：固定兩趟水源距離與城心評分 |
| `site_skeleton_roads.cpp` | S2：道路 edge → 城門、最多四條 A* 主幹道與路段重用 |
| `site_skeleton_blocks.cpp` | S3 資料驅動遞迴二分街廓；S4 水／路／坡度可建地遮罩 |
| `site_population.cpp` | F1～F5 編排、程序層版面／有牆必有門驗證與填充雜湊；不准回寫骨架 |
| `site_fill_detail.h` | F1～F5 內部共用介面與座標／邊 helper |
| `site_zoning.cpp` | F1：人口／建設等級配額、街廓偏好評分與 deterministic 配對 |
| `site_buildings.cpp` | F2：資料 def 選取、密度目標與單趟臨街貪婪 packing |
| `site_building_placement.cpp` | F2：旋轉 footprint、臨街、可建地與占用合法性 |
| `site_fortifications.cpp` | F3：街廓外緣牆環、主幹道城門、雙重牆、塔樓與護城河 EdgeDef |
| `site_landmarks.cpp` | F4：owner 勢力風格表選取、城心附近最高分街廓放置地標 |
| `site_damage.cpp` | F5：牆缺口與依城門／缺口距離優先的瓦礫／焚毀狀態 |
| `site_materialize.*` | 依聚落有無分流城區／荒野，首次／冷重算展開、ZoneManager callback 接縫、Frozen 凍結／解凍與持久層收回 |
| `site_streaming.*` | 經 ZoneManager 單一取得入口升載；玩家跨格重算 3×3 FULL／5×5 COARSE 場，尾端升降級、延遲逐出並批次推進 FULL Site |
| `site_reduction.*` | 固定 row 的 Site→Region 歸約 |
| `site_event_escalation.*` | 建築事件先落持久來源；達 Region 級才立即同步歸約快變數 |
| `site_build_loop.*` | `L_FULL` 批次逐小時／全局時鐘旬界編排、`ZoneKey` 正規化、工地命令、持久 `CityBuildState` 與 Region 回填 |
| `site_build_economy.cpp` | 建造進度、每小時糧食／生產、相鄰效果與人口成長 |
| `site_build_loop_detail.h` | 城建命令與每小時經濟結算的內部介面 |
| `site_lifecycle.*` | `SiteDigest`、持久物件閉式補算／飽和，以及骨架失效時的保留／就近搬移／災變毀壞與敘事事件 |

`site_materialize.*` 另提供 M4 的時鐘式 `unload_site_zone`／重載補算 overload；既有
`collapse_site_zone` 保留 M2 無時間往返語意。`site_unload_equivalence_test.cpp` 以
N=1/5/20/100 旬驗證 L_FULL／L_ABSENT 的四列歸約誤差、持久物件集合、負向控制與 24 旬老化上限，
並固定 20 旬量測 0/2/5/10/20 次卸載的偏差。`site_migration_test.cpp` 人工改 Region 慢變數後冷重載，
驗證三種遷移結局、無非法 live 座標、毀壞／事件持久化，以及只有搬移時仍會產生敘事事件。

`data/site_projection.toml` 保存 Terrain→Ground 映射與城區骨架規則；`data/site_city.toml`
保存 F1/F2 配額與建築、F3 城防引用、F4 勢力風格及 F5 缺口比例。載入型別分別是
`rules::SiteGenerationRules` 與 `rules::SiteFillRules`，已啟用分區缺配額或建築 def 時載入失敗。
`data/site_build.toml` 另存城建建造時數、產出、容量、相鄰效果與人口成長參數。

### 荒野 W1～W6

| 檔案 | 職責 |
|---|---|
| `site_wilderness.h` | `BoundaryProfile`、荒野慢／快分段輸出與不進存檔白名單的 live component |
| `site_wilderness.cpp` | W1～W6 編排、W5 唯一快變數入口與結果驗證 |
| `site_wilderness_boundary*.{h,cpp}` | 規範 edge／corner id、共享邊界剖面與 Region 慢變數投影 |
| `site_wilderness_terrain.cpp` | W1：四邊內插、relief 噪聲、海域與通行遮罩 |
| `site_wilderness_pathfinding.cpp`、`site_wilderness_paths.cpp` | 有界 A*；W2 河流／湖泊與 W3 道路／橋 |
| `site_wilderness_content.cpp` | W4 抖動網格植被、廢墟街廓複用與 W6 L3 入口 |
| `site_wilderness_entities.cpp` | 完整荒野 hash 與 W4～W6 live registry 實體安裝 |

`data/site_wild.toml` 保存荒野坡度、密度、入口與廢墟保留比例。對應測試是
`site_wilderness_{boundary,generation,content,lifecycle}_test.cpp`：共享邊五項判準、W1～W6
分支證據、海域／廢墟變體、零玩法持久物與冷重算，以及 Debug／Release 30 ms 預算。

對應測試在 `tests/site/`：`site_skeleton_test.cpp` 驗 S1～S4；`site_fill_test.cpp` 驗快慢隔離、
有牆必有門／雙重牆、owner 地標、損毀距離偏好、packing 與 S+F 效能；既有 `site_materialize_test.cpp` 的持久
建築座標測試會在真遮罩上重跑，`site_roundtrip_test.cpp` 驗三次冷往返與非空程序層重算。
`site_build_{loop,persistence}_test.cpp` 驗人口演化、批次旬界只結算 Region 一次、輸入順序無關、
單 Site 等價、兩種擺法、絕對歸約不重複與 pending 冷載。
