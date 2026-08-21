# M5.2 完成回報 — Local 路線 A：建築內外與垂直層

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

**原任務書**：[m5-2-route-a-buildings.md](done/m5-2-route-a-buildings.md)

## 結果

已完成路線 A 的 A1～A7，帶 `structure` 的 Site tile 現在會展開成一個 64×64 街廓，
包含沿外圈排列的房屋、中央內院、房間、邊屬性牆／門／窗、資料驅動家具、延遲居民與
z=−1/0/+1 垂直層。路線 B 保留原分派。沒有修改 `design/`、`core/world/`、
`core/site/site_build_loop.*` 或存檔版本欄位，也沒有實作任務書排除的功能。

## 驗收實測

固定住宅輸入 `site_seed=0x5A17` 的結果如下：

| 判準 | 實測結果 |
|---|---|
| 尺度對應 | 15 棟房、地面 60 間房、三層合計 124 間；所有房間寬高皆 ≥5 格；中心座標未落入房屋，保留內院 |
| A1～A5、A7 路徑 | 108 扇門、51 扇窗、65 件家具、3 個 z 層、16 個垂直連結，全部非零 |
| 居民延遲具象化 | 未進屋時統計 68 人、居民實體 0；進第 1 棟後才生成 6 個 Ambient 居民，程序實體記錄由 65 增至 71；重複進屋不重複生成 |
| 實體數 | 未進屋住宅為 65 個程序家具實體記錄；商業固定輸入為 88，維持幾十量級而非按全城人口生成 |
| 門牆不變式 | 每個房間邊界至少一扇門；門的 `EdgeDef` 同時具 wall/gate/openable；相鄰格的邊雙向一致 |
| 決定性 | 同輸入正規化雜湊皆為 `16677544010598737520`，改變 seed 後雜湊不同；雜湊逐欄位計算，未使用物件 byte 相等 |
| 骨架只讀慢變數 | 公開簽章只接受 `LocalSlowVars`；編譯期測試確認不能以 `LocalFastVars` 呼叫 |
| 10 ms 預算 | Debug、商業路線、固定暖機後 min-of-5：`1.34689 ms`；該次確實生成 15 棟、124 間、108 門、88 家具／實體記錄 |

Site 固定見證仍為 32 個街廓，面積 min/median/max = 21/96/322。

## 負向控制與共用證據

- 封死房間：把生成結果中的 216 個門邊儲存槽換成純牆後，
  `valid_building_invariants` 回傳 false，證明測試能抓到無門房間。
- 共用切分故障注入：暫時把 `recursive_partition.cpp` 的終止條件改成少遞迴一層後，
  Local 地面房間由 60 變 30、Site 街廓由 32 變 16，兩個固定見證同時紅；已還原並重建。

## 新機制與原抽象缺口

1. Site 的遞迴二分原本藏在 `site_skeleton_blocks.cpp` 私有實作，演算法概念可複用，
   但程式邊界不可複用。因此抽成 `core/spatial/recursive_partition.*`；Site 以一格街道為
   separator，Local 以零格 separator 表達兩房間間的邊。
2. `LocalPayload` 原本只容納單一地面 `tiles`，無法表達 `lowmap.md` 已定義的同 zone
   垂直層；改為 `z → LocalTiles` map。它仍是程序層，沒有新增持久欄位或變更存檔版本。
3. A2～A7 的尺度、機率、EdgeDef 引用與家具表新增為 `data/local_buildings.toml`，
   避免把可調規則寫死在生成器。

## 現有測試證不了／需規劃者裁定

- `LocalSlowVars` 沒有 owner，且骨架又被要求不得讀控制方快變數；目前門型只依 zoning
  分住宅／商業，無法同時兌現 `localgen.md` 的 owner 風格。需裁定風格鍵是否為慢變數，
  或把 owner 視覺差異留給骨架後的易失／顯示階段。
- Site→Local 的慢投影沒有可用的人口密度；居民統計目前依資料檔的每戶 3～6 人生成，
  測試只能證明延遲具象化與數量上限，不能證明它隨 Site 人口密度變化。
- 「玩家進屋」事件接線屬串流／探索協調器，本任務只提供可重入的指定房屋具象化入口；
  具名 NPC 還原屬明列排除的持久層疊加，均未順手實作。
- 1.34689 ms 是本機 Debug 固定資料的生成成本，不等同所有硬體、Ruleset 與最佳化建置上界。

## 驗證

- `cmake --build build --parallel 2`：通過。
- `ctest --test-dir build --output-on-failure`：217/217 通過，82.38 秒；
  `CoreIsolation.CompileCommands` 通過。
- `./build/aetheria_sim --tick 62208000`：通過，抵達 year 3 / season 1 / month 1 / xun 1。
