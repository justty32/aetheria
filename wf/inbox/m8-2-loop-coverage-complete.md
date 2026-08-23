# M8.2 完成回報 — 三層玩法接進迴圈

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者

## 結論

Region → Site → Local → 負 z 地城已接到同一個純 C++ `PlayableSession`，每層可返回，
也各有親自／交給系統入口。Godot 只送命令及重讀批次快照。實際 Godot 視窗已用滑鼠
從 Region 地圖格走到 z=-3、清空、逐層返回，再重進地城確認持久性。未改 `design/`、
`core/script/`、`scripts/`；`kSaveFormatVersion` 仍為 **20**。

## 滑鼠操作腳本

1. 在 repo 根執行 `godot-mono --path godot --resolution 1400x900`。畫面為 Region，
   霧橋鎮初值 `建設=1、治安=20`，清剿與地城需求皆為 true。
2. 直接點 Region 地圖上標示的 `(60,48)` 格（不是 headless 命令），進入 Site 城區。
   先按「free 後從 core 重建本層畫面」，再按「蓋一棟住宅（24 小時）」；看到
   `建設 1→2`。按「接受『清剿盜匪』湧現任務」，看到 `已接=true`。
3. 按「親自進 Local」，先按本層 free 重建，再按「開門並向東走一步」；事件數字
   顯示 x `32→32`（事件欄的 a/b 是移動後 x/y，玩家確實從 x=31 移至 x=32）。
   按「親自清剿盜匪」，看到治安 `20→50`，清剿可用／已接都變 false。
4. 按「親自下負 z 地城」，先按本層 free 重建；按「深入下一個負 z 層」兩次，
   事件依序顯示 `-1、-2、-3`。按「觸發機關、擊敗 Boss、清空地城」，看到
   `cleared=true`、敵人密度 `72→12`，地城需求 false。
5. 依序按「返回 Local 地表」→「返回 Site」→「返回 Region」。再次直接點 `(60,48)`，
   再進 Local／地城；重進截圖仍顯示 `cleared=true、72→12`。再逐層返回 Region。
6. 在 Region 按「Site 往返三次」、「N=100 期望值」、「締結和平條約」，看到三個
   hash 相同、`48000 vs 48000；+0.000%`、和平條約 `0→1`。Region 也以滑鼠按過
   free 重建。

## 實際視窗截圖

- [Region 最終狀態](../../godot/artifacts/m8-2-region.png)：建設 2、治安 50、
  cleared、三次 hash、N=100 與條約 1 同畫面。
- [Site 城區](../../godot/artifacts/m8-2-site.png)：住宅完成與任務已接。
- [Local 探索](../../godot/artifacts/m8-2-local.png)：FOV／開門後位置、治安 20→50、
  清剿需求消失。
- [地城清空](../../godot/artifacts/m8-2-dungeon.png)：z=-3 事件、cleared 與 72→12。
- [清空後重進](../../godot/artifacts/m8-2-dungeon-reenter.png)：退出到 Region 後重進，
  cleared 與低密度仍在。

五張皆為 1400×900 實際 X11 Godot 視窗，由 `xdotool mousemove ... click 1` 逐鍵操作後
以 `import -window` 擷取；不是 headless 產圖。SHA-256 依上列順序為：

```
d9042076db31caef4d6b193f4d5035f5b13a83cb21ea198ad601bfa2caaacc98
673ecb52ef61d973c981d7e177e0dd9a0cf5a731fc1c10589e0d3510f7299835
f6ba61f62dc1839da8181632d20a89ebb91510ca12239c6592d5e7d530819570
d320a9e3113582cec6efce6f0e59614906526a0c5f4420e2ea2479105215b0e8
75b99297da6e43d7580728a156b6dff1f25a86829f7ea32ef2f64ec04c09f84a
```

## 上層回寫數字

| 行為 | 前 → 後 | 額外證據 |
|---|---:|---|
| Site 完成住宅 | Region 建設 `1→2` | Site 載入時完成建築數 `0→1`；回 Region 後 full CityBuildState 已卸載，但 Region 建設 2 保留 |
| Local 清剿 | Region 治安 `20→50` | 任務總數 `3→2`，清剿可用 true→false、已接 true→false |
| 地城清空 | 密度 `72→12` | cleared false→true；退出到 Region 再重進仍相同 |
| 湧現任務完成 | 地城需求 true→false | 任務總數 `2→1` |
| 外交 | 條約 `0→1` | 使用既有 `WorldDiplomacyState` |

Site 三次往返的視窗量測 hash 為
`9628997528793589803 / 9628997528793589803 / 9628997528793589803`。
獨立 core 測試在初始世界的三值則皆為 `15863864288219402234`。

**請規劃者裁定的既有模型接縫**：M3 `CityBuilding` 與 M2
`PersistentBuilding` 尚無正式型別映射，而目前 Development reduction 只接受
`SettlementHall`。為遵守不改存檔版號／不新增玩法，本輪在住宅完成後加一個既有
`SettlementHall` projection marker，再走正式 ReductionTable；因此它同時帶來既有
人口貢獻。若未來要讓不同 CityBuilding 有不同 Region 權重，應另開設計與 schema 任務。

## 親自／系統與負向控制

非戰鬥城建產出固定做一次 N=100，沒有試別的 N 或調參：親自總產出 **48000**、
系統總產出 **48000**，signed relative error **+0.000%**，低於 5%。兩路都以相同
SiteTurnPipeline 跑 240 小時的 `city.workshop`，差別只在駐留選擇。

`acquire` 負向控制真的紅：暫把 `acquire_coverage_site()` 降成 `load()`。第一次完整入口
先紅於 `enter_full_site 要求 Region tile 已標記 live`；為量到任務指定的斷言，負向觀察
夾具暫略過 `enter_full_site`，同一測試 exit **1**：

```
Expected equality of these values:
  value.site_view()->cells.size()
    Which is: 0
  4096U
    Which is: 4096
```

之後已還原為 `ZoneManager::acquire` 並重建、重跑正向測試。這裡沒有反覆試任何數值。

## free 重建與驗證

用滑鼠在 Region、Site、Local 地表、Local 地城各按一次重建，Godot stdout 為：

```
PLAYABLE_REBUILD_LAYER=Region 大地圖 MATCH=1
PLAYABLE_REBUILD_LAYER=Site 城區 MATCH=1
PLAYABLE_REBUILD_LAYER=Local 探索 MATCH=1
PLAYABLE_REBUILD_LAYER=Local 地城 MATCH=1
```

以無既存按鈕 focus 的基準畫面比對，四層 ImageMagick `compare -metric AE` 均為
**0**。另一次在「締結條約」鍵仍有 focus 時直接取 Region 圖，只有按鈕狀態造成非零，
沒有拿該次充當內容比對。輸出中沒有 `was freed or unreferenced while a signal is being
emitted from it`；重建改為先移出樹再 `queue_free()`。X11 僅有輸入法及 llvmpipe
V-Sync 環境警告。

- `cmake --build build --parallel 2`：通過。
- `ctest --test-dir build --output-on-failure`：**404/404**，92.88 秒。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- Godot editor 首掃與 headless 主場景：exit 0。
- `git diff --check`：通過。

自動測試能證明 core 回寫、持久性、acquire、hash 與校準，不能證明地圖格真的可點、
逐層畫面可讀或 signal 期間沒有同步 free；這三項由上述實際滑鼠流程、截圖、AE 與
Godot stdout 補足。
