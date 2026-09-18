# godot 端現況 — 離「一個能玩的遊戲畫面」差多遠

← [investigation](README.md)

## 問題

使用者原話：「godot 端現在是什麼？離「一個能玩的遊戲畫面」差多遠？」
五條可判定提問：(1) godot/ 盤點；(2) 跑起來看到什麼；(3)
`player-residence.md`「三層各一套場景＋鏡頭轉場」與 `tech-stack.md` 鐵律守住了嗎；
(4) M8 判準做到幾成；(5) 更新機制與效能坑。

## 結論

godot 端不是遊戲畫面，是一個「core 除錯控制台」：一個 `Node` 場景、
864 行 GDScript、零美術素材，全部 UI 在 `_ready()` 裡用程式碼堆出來
（`godot/main.gd:64-157`）。M8 判準「只用滑鼠鍵盤打完一場仗並看到世界改變」
**功能上做到了、體驗上沒有**——仗打得完、數字會變，但「看到世界因此改變」
靠的是側欄一行 `世界：owner %d→%d；人口 %d→%d；治安 %d→%d` 的文字
（`main.gd:388-392`），不是畫面。估**六成**：迴圈通、呈現層等於零。

## 現況

**盤點**：4 個 `.gd`（`main.gd` 502／`region_debug_renderer.gd` 219／
`region_debug_palette.gd` 104／`event_panel.gd` 39，共 **864 行**）、2 個 `.tscn`、
3 個 `.po`（各 **40** msgid）。`main.tscn:5-6` 是裸 `Node` 只掛 `main.gd`；
`event_panel.tscn` **無人 instantiate**，連同它用的 `poll_events()` 一起是死路。
`aetheria.gdextension:3` 以 `entry_symbol = aetheria_library_init` 載入
`res://bin/libaetheria_bridge.so`（107 MB，建於 2026-08-23）。
`project.godot:5-7`：`aetheria Playable Battle Loop`、1280×900、`gl_compatibility`；
**沒有 `[input]` 段**。

**畫面**：視窗 = 左側 510 px 文字側欄（`main.gd:73-147`）＋右側一張
`TextureRect`（`main.gd:149-157`）。
Region 層是 **128×96 = 12,288 格**（`core/runtime/playable_session.cpp:110`）
每格 8 px 的假色柵格（`region_debug_palette.gd:4`），即 1024×768 `Image`：
地形／起伏／owner 底色＋國界、地物、河流道路 edge、聚落、portal、
單位 4 px 方塊（`region_debug_renderer.gd:25-59`）。
`main.gd:5` 的 `LAYERS` 把海拔／溫度／濕度寫死關掉，**且無開關 UI**。
Site／Local／Dungeon 是 64×64（`core/site/site_projection.h:22-23`）的 **7 色**
方塊圖（`main.gd:268-285`）。**輸入只有一種**：地圖左鍵點擊（`main.gd:156,
326-352`）——點覆蓋格＝進 Site、點我軍＝選取、再點別處＝送移動意圖。
**沒有鍵盤、沒有相機、沒有縮放平移**（`.gd` 對 `Camera2D`／`TileMap`／
`_input`／`_process` 零命中）。

**設計 vs 現況**：
<!-- wf-nav -->
- 「**三層各一套場景**」→ **0 套**：三層共用同一個 `TextureRect`，切層走
  `_rebuild_for_residence()`（`main.gd:318-323`），側欄 `queue_free()` 再重建。
- 「**鏡頭轉場**」→ **沒有**：無 `Tween`、無 `Camera`，硬切。
- 鐵律「core 不依賴 godot-cpp」→ **守住**：`core/` 對 `godot` 零命中（0 行）。
- 鐵律「`bridge/` 只准 include `core/api/`」→ **破了**：8 個 core include 裡只有
  `core/api/version.h` 合規，其餘直接吃 `core/runtime/`、`core/world/`、
  `core/worldgen/`、`core/rules/`、`core/narrative/`
  （`bridge/aetheria_core.h:3-4`、`bridge/aetheria_core.cpp:3-8`）。
- 「駐留層不准存在 Godot 端」→ **守住**：`PlayableResidence` 住在
  `core/runtime/playable_session.h:31`，GDScript 每次從快照重讀。
- 但**有玩法狀態住在 `.gd`**：`_selected_unit_id`（`main.gd:33`）core 不知道，
  而自證用的 `_rebuild_view()`（`main.gd:410-421`）只 free `_view`、**不 free 持有它的
  `main.gd` 節點**——「free 再從 core 重建」這條自我檢查跑的是子集。
- **枚舉文字寫死在 GDScript**：`main.gd:6-16` 共 20＋7＋4＋5＋4 = **40** 個字串，
  逐格對應 core 枚舉順序（`playable_session.h:33-52` 正好 20 項）——插一個就錯位。
- **i18n 是空殼**：`project.godot:17` 載入三個 `.po`，但 `main.gd` 的 `tr()` 呼叫是 **0**。
- **零美術**：`godot/` 只有 7 張 `artifacts/*.png` 截圖，無 sprite／tileset／字型；
  M9.2 美術管線整包在 `tools/art/`，無產物進 `godot/`。

**M8 判準**：可點的東西 = **18 個** `coverage_command`（`main.gd:163-190`）
＋選取／移動／推進一旬／親自指揮／讓系統算。
一場仗：選我軍 → 點敵軍格 → 推進一旬 ×2 → 遭遇時側欄冒出「親自指揮（Site）／
讓系統算（Region）」兩顆鈕（`main.gd:127-130, 261-262`）→ 按下去 → 側欄戰報顯示
傷亡、具名人物命運、`owner／人口／治安` 前後值（`main.gd:383-393`）。
世界真的變了、owner 底色也真的重畫——**但無動畫、無音效、無提示、無鏡頭移動**。

**更新機制**：**不是 poll，也不是每 frame**——沒有 `_process`。每個按鈕回呼結尾叫
`_refresh()`（`main.gd:235-265`），它呼叫 `get_playable_snapshot()` 把**整張圖重打包**：
12 個 Packed 陣列 ＋ 122,880 bytes 的 `tile_blob`（`bridge/aetheria_core.cpp:394-417`；
12,288 格 × 10 bytes，M8.1 視窗實測打包 **2.060 ms**，
見 `wf/inbox/m8-1-playable-loop-complete.md:66-68`）。
`revision` 有從 core 送上來（`bridge/aetheria_core.cpp:385, 454, 507, 605`），
但 GDScript 只拿它印在狀態列（`main.gd:249-251`）與負面探針比對
（`main.gd:478, 485`）——**從未用來跳過重畫**。

## 差距

13 條差距（#／差距／佐證）已抽到 [godot-pipeline-gaps.json](godot-pipeline-gaps.json)。

## 牽扯到的部份

- **檔案**：`godot/` 的 8 個原始碼／設定檔＋3 個 `.po`；`main.gd` 502 行含全部 UI 與回呼。
- **bridge**：`aetheria_core.cpp:264-290` 綁 **10** 個方法，`main.gd` 只用 **7**；
  `get_core_version`、`generate_region`、`poll_events` 無人呼叫。新增快照欄位
  都得改 `get_playable_snapshot()`（`aetheria_core.cpp:394`）。
- **判準**：M8（`design/milestones.md:31`）、M9「關掉 core 重開，畫面與音景完全一致」
  （同檔 :33）——後者現在無法測，因為 `_rebuild_view()` 不 free 持有狀態的節點。
- **設計文件**：`design/maps/player-residence.md`「對顯示層的要求」整節未實作；
  `design/architecture/tech-stack.md`「Godot 端做什麼」列的 TileMapLayer／相機／小地圖／
  對話框／音效 **0 項**。
- **並行工作**：本報告只看 main 分支；`../aetheria-wt-m10-0c` 正在往 `bridge/`／
  `godot/` 加存讀檔入口，第 6、7 項差距會與它的新入口相撞。
