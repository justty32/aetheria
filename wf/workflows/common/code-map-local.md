# code-map — Local 與 Spatial

← [code-map](code-map.md)

## `core/local/` — L2→L3 生成、展開與探索

本目錄從 Site tile 投影慢變數與 local seed，依 structure 分派 A／B／C 生成路線，
落到 LocalPayload 的 z → LocalTiles 程序層集合。LocalTiles 是單一 z 層 SoA，
玩法狀態在純 C++ 核心，Godot 不持有副本真值。

`local_<職責>*.cpp` 按投影、生成、展開、探索、戰鬥與歸約分群；`*_detail.h` 只共享群內 helper。
`local_reduction.*` 量測建築段、控制點、採集點、通道四列，驗 Local key／LOD，
經對應 ReductionTable 唯一 writer 套用到父 Site tile。

## 生成與地城檔名群

`local_building_*.cpp` 是路線 A：地基內插與逐層編排、排屋／房間切分、門窗與垂直連結、
資料驅動家具、居民統計及進屋延遲具象化，並分出不變式與正規化 hash。
`local_generation*.cpp` 是路線 B：開放地表、道路／河流 crossing、散布與物件、驗證。

`local_underground_*.cpp` 是路線 C：地表 B、structure 深度與負 z 層編排，
依 mine／dungeon／ruin 後綴處理礦脈掘進、房間直折走廊及路線 A 拆除式遺跡；
geometry、validation、hash 群維持跨層 flood fill、垂直一致性與決定性。
`dungeon*.cpp` 另處理深度曲線、三種參數化房間／內容、機關互動、Boss 與光源壓力，
已觸發機關及已領寶藏歸 Local zone 持久狀態。

`data/local_buildings.toml` 管尺度、房間下限、牆／門／窗、垂直層機率、居民及家具表；
`data/site_city.toml` 的特殊 structure def 決定地下種類及 1～16 層深度，不進一般城市配額。
`data/edges.toml` 的 EdgeDef 延用於路線 A，門／窗仍是邊屬性。
`data/dungeon.toml` 管三種地城參數、深度／光照／cleared 曲線與四類 TrapDef，載入時驗證。

## 探索與戰鬥

navigation 群提供跨 zone 格位址、canonical 邊位址與可注入門查詢，
LocalDoorState 是 registry 上持久化的已開門 edge 集合。
`local_fov*.cpp` 分視野編排與整數 DDA 射線，只以 `peek_edge` 判斷牆／門遮蔽；
movement 群做四鄰接步判定，區分開門、上鎖、牆與未知邊界。

`core/local/local_path.h` 是邊感知整數 A* 門面，保留 enum，彙整 `local_path_types.h`、
`local_path_detail.h`、`local_path_impl.h` 的型別、helper 與 inline 實作。
只有未載入目標才退化為 Site 粗路徑。

`local_combat*.cpp` 分 6 秒逐單位回合、導航／攻擊及結果歸約；
強制接受 Site 入口／撤退邊，復用 FOV、A*、移動、d100 與 significance δ，
歸約三層士氣、撤退／追擊／潰散，不可自行選入口或繞過 M5 視野。

## `core/spatial/` — 兩層共用純演算法

`core/spatial/boundary_profile.h` 是 canonical edge／corner、角錨定與剖面生成門面，保留 enum；
`boundary_profile_types.h`、`boundary_profile_detail.h`、`boundary_profile_impl.h`
分型別、內部 helper 與 inline 實作，Site／Local 共用同一邊界規約。

`recursive_partition.*` 讓 Site 街廓以一格街道切分、Local 房間以格邊切分，
共用有界遞迴實作，只換 config。
`core/spatial/reduction.h` 是 Site→Region 與 Local→Site 共用 typed storage、optional
絕對快照套用與決定性 hash；兩層各自 ReductionTable 仍是唯一 writer。

## `tests/local/` — 證據導航

測試依 `<主題>_<子題>_test.cpp` 分組，`*_test_support.h` 放本目錄 fixture／helper。
building 群驗尺度、房間門不變式及封死反例、家具／居民數、垂直層、正規化 hash 與 10 ms 預算；
共用切分的另一層見證在 Site skeleton 群。
underground 群驗三種 structure 深度、全格／房間可達性、單走廊封死負向控制、
60～80% 遺跡拆除、垂直雙向一致、hash 與 10 ms 預算。

fov 群驗遮蔽負向控制、光照、門、跨 zone 退化、對稱性、決定性及暖機後 min-of-5；
movement／path 群驗四鄰接、門鎖、未載入鄰區、連通性與障礙。
combat 群驗 Site 邊界權威、6 秒／5 格、M5 三接點、d100、significance δ、士氣／撤退／潰散、
N=1000 真 Site 校準及 2 ms 預算。`LocalCombatNegative.RequiredFailures` 確認入口與 FOV 故障注入真的紅。

reduction 群以非空四列、空 Local 無寫入、200→175→150 絕對快照與語意順序無關 hash 驗 Local→Site；
共用 apply 故障注入會與 Site→Region 同時紅。
dungeon 群驗 100 座線索相關性、共用深度曲線、引敵觸發、Boss 破階循環、v20 持久層、
cleared 十次產出、光源耗盡分布、歷史來源與 min-of-5。
