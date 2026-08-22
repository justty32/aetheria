# M8-INT-6 完成回報 — 併入 M8.1 可玩迴圈

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者

## 結論

`m8-1-wt` 已以 `--no-ff` 併入。我在實際 Godot 視窗中只用鼠標分別走完 Site
親自指揮與 Region 系統計算兩路，兩路都完成遭遇、結算並看到暮橋鎮的世界真值
改變。未改 `design/`、`core/local/local_combat*` 或玩法／UI 行為；
`kSaveFormatVersion` 仍為 **20**。

## 衝突解法

`cmake/targets_core.cmake` 的衝突中，M7.3 `local_combat.cpp` 與 M8.1
`playable_session.cpp` 共用了衝突塊外的 `)`。我先在 M7.3 區塊補上結尾 `)`，
再開一個獨立、括號完整的 M8.1 `target_sources` 區塊。`git diff --check` 通過，
CMake configure 沒有 parse error。

## 使用者操作腳本（七步）

1. 在 repo 根目錄執行 `godot-mono --path godot`。看到「AETHERIA — 可玩戰役迴圈」、
   128×96 張 Region 地圖、日期「1 年／季 1／月 1／旬 1」；暮橋鎮 (63,48)
   是 `owner=2／人口=100／治安=20`，狀態列顯示批次拉圖 122880 bytes。
2. 按「1 選取我軍」。看到黃字「已選取青色我軍 #1001」，地圖上我軍出現選取框。
3. 按「2 移動至敵軍」。看到「移動意圖已送 core：部隊 #1001 → 敵軍所在格
   (65,48)」，revision 由 1 變 2；此時只下意圖，部隊還沒有被 Godot 移動。
4. 按第一次「推進一旬」。看到日期變第 2 旬、我軍 `(61,48)→(62,48)`、
   敵軍 `(65,48)→(64,48)`；黃字顯示「七階段=7、AI 行動=2」，事件欄顯示
   勢力 2 備戰、勢力 3 宣戰及「敵軍移動：x 65→64」。
5. 再按一次「推進一旬」。雙方到 `(63,48)`，看到「遭遇敵軍！」與
   `戰力 120000 vs 48000`；旬按鈕停用，「親自指揮（Site）」和
   「讓系統算（Region）」同時出現。
6. 擇一條結算：按 Site 看到我軍傷亡 **7517**、敵軍傷亡 **26518**；或按
   Region 看到我軍傷亡 **7024**、敵軍傷亡 **26828**。兩者都顯示
   「敵軍潰散」、士氣 `-5/-76`、「守軍隊長艾琳 — 負傷」與各自的 core 來源。
7. 確認戰後暮橋鎮顯示 `owner 2→1；人口 100→34；治安 20→0`，地圖 owner
   顏色也隨 core 快照重畫。按「開新遊戲」後可重做第 2∼6 步驗另一路；
   戰後再按「free 後從 core 重建整個畫面」，日期、戰報、事件與世界真值保持一致。

## 視窗實測與截圖

- [親自指揮](../../godot/artifacts/m8-1-manual.png)：1280×900，SHA-256
  `6a7a7fc5e356ab93aab932174d641a31c63e5b14521d5a77094856b7a7264488`。
- [系統計算](../../godot/artifacts/m8-1-auto.png)：1280×900，SHA-256
  `6fc54baf691646b4c2c2731501d1b7da62209f5db115b623f3b45b5bac5230db`。
- 兩次實際視窗都以鼠標逐顆按鈕操作；截圖是戰後實際視窗，不是 headless 數字。

## 批次、重建與邊界

- 批次拉圖：完整 12288 格的 `PackedByteArray` 為 **122880 bytes**；每次 refresh
  為 **1 次 bridge 呼叫**。manual/auto 探針實測打包 2.086387/2.063985 ms。
- free 重建：manual 與 auto 皆 `PLAYABLE_REBUILD_MATCH=1`；實際鼠標按 free 後與重建前
  截圖的 ImageMagick AE 皆為 **0**。鼠標觸發時 Godot 會額外警告釋放正在發 signal
  的物件；進程與畫面仍正常，已以 `.codex-inbox/m8-int-6.ask` 告知規劃者，
  本輪遵守只做整合而不擴修 M8.1 UI。
- bridge 域外座標 `(-1,999)`：錯誤「座標超出 Region 邊界」，拒絕=1，
  revision **1→1**。域外 tick `INT64_MAX` 回空 Dictionary，`BRIDGE_INVALID_TICK_EMPTY=1`，
  同進程繼續輸出 `BRIDGE_PROCESS_ALIVE=1` 並 exit 0。
- core 圖內海格：錯誤「移動命令目標不可通行」，拒絕=1，revision **1→1**。
- **負向控制真的紅**：故意預期域外座標會被接受，實際 exit **1**，紅字為
  `NEGATIVE_CONTROL_RED：預期非法座標被接受，實際錯誤=座標超出 Region 邊界`。

## 驗證

- `cmake --build build --parallel 2`：冷建置通過。
- `ctest --test-dir build --output-on-failure`：**402/402** 通過，83.60 秒。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- Godot headless editor 首掃、headless 主場景：均 exit 0。
- `git diff --check` 與 staged diff check：均通過。
