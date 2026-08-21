# code-map — `core/site`

← [總 code map](code-map.md)｜[conventions](conventions.md)

`core/site/` 是 L1→L2 的界面與城區生成邊界，全部為純 C++，不得依賴 godot-cpp。

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
| `site_population.cpp` | F 階段編排、程序層版面驗證與填充雜湊；不准回寫骨架 |
| `site_fill_detail.h` | F1／F2 內部共用介面與座標 helper |
| `site_zoning.cpp` | F1：人口／建設等級配額、街廓偏好評分與 deterministic 配對 |
| `site_buildings.cpp` | F2：資料 def 選取、密度目標與單趟臨街貪婪 packing |
| `site_building_placement.cpp` | F2：旋轉 footprint、臨街、可建地與占用合法性 |
| `site_materialize.*` | 首次展開、冷重算展開、收回與持久建築座標疊加 |
| `site_reduction.*` | 固定 row 的 Site→Region 歸約 |
| `site_event_escalation.*` | 建築事件先落持久來源；達 Region 級才立即同步歸約快變數 |

`data/site_projection.toml` 保存 Terrain→Ground 映射與城區骨架規則；`data/site_city.toml`
保存 F1 住宅／商業配額、F2 密度與建築尺寸 def。載入型別分別是
`rules::SiteGenerationRules` 與 `rules::SiteFillRules`，已啟用分區缺配額或建築 def 時載入失敗。

對應測試在 `tests/site/`：`site_skeleton_test.cpp` 驗 S1～S4；`site_fill_test.cpp` 驗快慢隔離、
村莊／大城配額、臨街無重疊 packing、院落與 S+F 效能；既有 `site_materialize_test.cpp` 的持久
建築座標測試會在真遮罩上重跑，`site_roundtrip_test.cpp` 驗三次冷往返與非空程序層重算。
