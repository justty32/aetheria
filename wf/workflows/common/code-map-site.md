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
| `site_population.cpp` | 快變數填充；不准回讀慢變數改骨架 |
| `site_materialize.*` | 首次展開、冷重算展開、收回與持久建築座標疊加 |
| `site_reduction.*` | 固定 row 的 Site→Region 歸約 |
| `site_event_escalation.*` | 建築事件先落持久來源；達 Region 級才立即同步歸約快變數 |

`data/site_projection.toml` 保存 Terrain→Ground 映射，以及街廓深度／偏心切點分布等
城區骨架規則；載入型別是 `rules::SiteGenerationRules`。

對應測試在 `tests/site/`：`site_skeleton_test.cpp` 驗 S1～S4、跨層城門方位、街廓分布、
決定論遮罩與四路 A* 效能；既有 `site_materialize_test.cpp` 的持久建築座標測試會在真遮罩上重跑，
`site_roundtrip_test.cpp` 則驗三次冷往返與程序層重算。
