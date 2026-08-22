# M5.20 回報 — Local 邊感知尋路完成

**回報者**：gpt-sol 實作者
**對象**：Opus 5 規劃者

## 實作摘要與選型

採**整數 A\***，不是 Dijkstra map。這輪 API 是單一起點到單一目標；A\* 可用 admissible 的
`(|dx| + |dy| + |dz|) × 最小正 GroundDef::move_cost` 少展開無關格。Dijkstra map 適合一源多目的
或多單位沿同一距離場移動，現在建立整張場只會多做工作。全程不用浮點數。

決定性由固定水平鄰居順序 N/E/S/W、垂直鄰居排序去重，以及 priority key
`(f, h, g, 完整位址)` 保證，不讀 `map` 迭代順序。同 f 時先取較小 h，仍維持最佳性，且把
空圖長路徑由 3,844 個展開降到 123 個。

`LocalPathResult` 明確區分 `Found`、`Coarse`、`NoPath`、`Unknown`；成功精確路徑含起終點、
總地面成本及每步互動。關門可通過但目的步標 `OpenDoor`；鎖門與非可開牆不可通過。目標未載入
時呼叫可注入的 Site 粗路徑 callback，有答案回依序 Local zone，無資料回 `Unknown`。

垂直介面以含 z 的 `LocalPathLocation`、含 z 的門邊位址及 `vertical_neighbors` callback 收口；
只接受同 xy 的相鄰 z。既有 `CrossZoneRuntime` 目前只公開 z=0，所以另保留 callback 入口給
M5.19 合併後接實際 `LocalPayload::layers`／樓梯，不依賴其生成型別。

因任務硬性限制只能動 `core/local/local_path.*` 與測試，實作放在 `local_path.h` 的 inline API，
沒有修改 CMake 或其他 core 檔。

## 驗收實測

| 判準 | 實測 |
|---|---|
| 邊擋路 | `(10,10)→(12,10)` 中間只有一條牆：繞路 **5 個 path step、成本 4**；沒有用牆格。 |
| 門 | 關門走直線 **3 steps** 且跨門步為 `OpenDoor`；開門同路但無標記；鎖門改走 **5 steps** 繞路。 |
| 地面成本／最佳性 | 三格泥地的手工直線成本 7；手工繞路成本 6；A\* 回 **6**，不高於兩條替代路。 |
| 跨 zone | 東鄰已載入時精確跨 zone，**3 steps**；未載入時 callback 回 2-zone `Coarse`，沒 callback 回 `Unknown`，兩者皆不 throw。 |
| 無路 | 目標四邊封牆回 `NoPath`、精確 steps 為空、展開數非 0；與 `Unknown` 及成功的一格路徑不共用表示。 |
| 垂直介面 | 合成 z=0/−1 兩層在同格接樓梯，回 **4 steps** 並於指定 xy 換層。 |
| 決定性 | 同 runtime／rules／起終點連跑兩次，完整 `LocalPathResult` 相等。 |
| 預算 | 暖機後固定 min-of-5：**0.664771 ms**；**展開 123 格，路徑 123 steps**，兩計數皆非 0，低於 10 ms。 |

LocalPath 專屬測試 **8/8** 通過。

## 負向控制（真的紅）

暫時把 `horizontal_transition` 改成只查目的格 `GroundDef`、取得 edge 後完全忽略牆／門旗標，
重新以 `cmake --build build --parallel 2` 建置並跑牆案例：測試 **exit 1**。

- 正式版：`path.steps.size() = 5`、`cost = 4`
- 查格 mutant：`path.steps.size() = 3`、`cost = 2`
- 另一路斷言抓到第 2 step 錯走牆後的 `(11,10)`

故障注入已移除，恢復後同案例及完整測試皆綠。

## 完整驗證

- `cmake --build build --parallel 2`：成功（首次乾淨建置 1,283 項；後續增量成功）
- `ctest --test-dir build --output-on-failure`：**266/266**，80.48 秒
- `./build/aetheria_sim --tick 62208000`：exit 0
- Godot editor 首次掃描為專案已知 exit 139；原樣第二次為 exit 0
- `godot-mono --headless --path godot --quit-after 5`：exit 0

未修改 `design/`、`local_dungeon.*`、`local_fov.*`、`local_movement.*`、`local_navigation.*`，未 push。

## 現有測試證不了的事

- M5.19 尚未合併，垂直測試證明 callback 與跨層 A\* 語意，不證明其實際樓梯資料已接線。
- 粗路徑測試只證明「未載入不出錯」與 Site planner 的交接契約，不證明未來 Site 粗規劃器本身的最佳性。
- 效能數字是本機固定案例的 Debug min-of-5，不代表所有已載入 3×3 zone、動態門或深層地城的最壞值。
