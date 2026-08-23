# core/worldgen 導航

← [code-map](code-map.md)｜[conventions](conventions.md)

## Region 十二階段生成

門面 `region_generator.h` → `region_config.h`（變數、常數、參數 hash）、各階段 header、`region_skeleton.h`／`region_diagnostics.h`；`field_redistribution.*` 是高度／濕度 identity 接縫。

內部共用：`gen_stage_ids.h`、`gen_grid.h`、`gen_noise.h`、`gen_hash.h`；`biome_classification.h` 隔離 terrain／relief 裁決。

實作：`region_seed.cpp`（種子推導與參數 hash）、`stage_plates/height/erosion/climate/rivers/biomes/features.cpp`（階段 1–7；量化閘口在 `stage_erosion.cpp`，地物約束在 `feature_placement.*`）、`civ_tiles.*`（人文階段共用底圖）、`settlement_scoring.cpp`＋`city_scoring.*`（共用純評分）、`city_selection.*`（canonical 分級選點）、`history_layer.cpp`＋`history_roads.*`（階段 8 選址／災變／古道）、`city_sites.cpp`（階段 9）、`road_path.*`＋`road_loops.*`＋`road_network.cpp`（階段 10 工程路徑／MST／補環路）、`portal_candidates.*`＋`portal_boundary_candidates.cpp`＋`portal_generation.cpp`（階段 11 候選、邊界落點與補路）、`capital_selection.cpp`＋`influence_claim.cpp`＋`governance_release.cpp`＋`influence_spread.*`＋`faction_generation.cpp`（階段 12 首都、全域認領、治理釋回與編排）、`region_build.cpp`＋`region_populate.cpp`（骨架／落地）、`region_stage_hash.cpp`＋`region_result_hash.cpp`（決定論 hash）、`region_debug.cpp`（診斷與灰階圖）。
