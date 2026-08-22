# 可複用元件與外部函式庫

> 從 [tech-stack.md](tech-stack.md) 拆出。那份講**程式碼怎麼分層、邊界契約與決定論**，
> 這份只回答一件事：**什麼可以拿現成的，什麼必須自己寫。**
> 通用原則見 [principles.md](principles.md)。

## 可複用元件來源

`~/repo/game_dev/my_godot_assists/` 已有多個現成的 Godot 元件，優先取用：

| 需求 | 現成元件 |
|---|---|
| 多 TileMapLayer 世界地圖 | `godot_world_map/` |
| 策略相機 pan/zoom/rotate/clamp | `godot_camera_rig/` |
| 選取／懸停描邊、框選 | `godot_selection_highlight/` |
| HUD 小地圖（含 fog、視野框） | `godot_minimap/` |
| 單位沿格移動 controller | `godot_character_controller/` |
| 角色紙娃娃換裝與動畫 | `godot_character/` |

`~/repo/game_dev/my-rpg-frontend/` 是既有的 Godot + GDExtension 專案，
`gdext/CMakeLists.txt` 與 `bin/*.gdextension` 的佈局可直接抄。

### ⚠ libtcod：評估過，**不採用**（M5.17）

`TCOD_MapCell` 只有 `transparent / walkable / fov` 三個布林，**遮蔽判斷是查格**
（`fov_symmetric_shadowcast.c` 的 `is_wall = !map_cell->transparent`），
而且**沒有「光線由哪格穿到哪格」的 callback**。

我們的牆是**零厚度的邊**：只擋東西向穿越、不擋同一格的南北進出。
把牆塞進任一側 tile 會**錯擋那個 tile 的其他三邊**；膨脹成整格就是設計已拒絕的浪費地圖。
兩倍解析度影子網格與 fork FOV 都要重做角點語意，**代價比自己寫還高**。

> **裁定：FOV／BSP／噪聲／RNG／呈現全部不借，維持自己寫。**
> 唯一值得借的是 **Dijkstra map 的概念**（Local 還沒有尋路）。

⚠ **這條不是「邊比較好」。** 傳統 roguelike 讓牆佔一格是**對它們的玩法夠用**——
牆可被看見、照亮、破壞、替換，ASCII／vault 編輯也是一字一格。
aetheria 的例外有具體理由：**同一套 `EdgeDef` 要統一河流、道路、城牆、房牆與門**
（`core/rules/def_types.h`），所以不能為了配合以格為前提的函式庫倒退資料模型。
