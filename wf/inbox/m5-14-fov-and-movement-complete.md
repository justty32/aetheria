# M5.14 完成回報 — 視野與探索移動

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**對應任務**：[m5-14-fov-and-movement.md](done/m5-14-fov-and-movement.md)

## 實作摘要與演算法選型

FOV 選擇「逐候選格的整數 DDA 光線」，沒有套用以牆格為前提的 shadowcasting：

1. 從格中心射到格中心，每次穿越格線只呼叫 `CrossZoneRuntime::peek_edge`。
2. 光線恰好穿過格角時，保守檢查角上的四條邊；反向光線得到同一邊集合，所以幾何完全對稱。
3. 候選範圍是圓形；暗／亮半徑由呼叫端傳入，格的 `light` 在兩者間做整數線性內插。
   一對端點取較大半徑，使 A→B 與 B→A 不因端點角色交換而改變。
4. 逐格射線是 O(r³)，但 Local 視野半徑有界；本輪 r=24 實測仍低於 10 ms。
   相較改造 shadowcasting，這個選型比較容易逐條證明「查邊」、角落與跨 zone 語意。

新增檔案：

- `local_navigation.*`：`LocalLocation`、跨 64×64 zone 換算、以排序端點規範化的
  `LocalEdgeAddress`、`DoorStateQuery`。
- `local_fov.*`：回傳固定掃描順序的可見格與 `degraded`。
- `local_movement.*`：四鄰接一步回傳 Allowed／MustOpenDoor／牆阻擋／鎖門阻擋／Unknown；
  探索 stride 直接引用 `time::kMinute`。

## 驗收實測

| 判準 | 實測證據 |
|---|---|
| 邊擋視線 | 可通行草地兩格間只放 `edge.house_wall`：牆後格不可見；正式版可見 **35 格**。 |
| `light` | 同一張空圖、暗／亮半徑 1／4：全暗 **5 格**、全亮 **49 格**，兩條路徑皆非空。 |
| 門語意 | 同一 `edge.house_door`：Closed 與 Locked 看不穿，Open 看得穿；移動分別為 MustOpenDoor、BlockedByLockedDoor、Allowed。 |
| 邊界退化 | 東鄰已載入時從 x=63 看得到鄰區 x=0；鄰區未載入時不 throw、跨界格不可見、`degraded=true`，本區仍有可見格。 |
| 對稱性 | 含 11 段牆的 6 個見證點做 36 個有序配對，**0 mismatch**；格角四邊規則不需要容許誤差。 |
| 移動合法性 | 空邊可走、牆不可走、閉門需先開、鎖門不可走；跨 zone 目的未載入回 Unknown。 |
| 效能 | 暖機後固定 5 次取最小：**9.50125 ms**，r=24 實際可見 **1,793 格**。 |
| 決定性 | 同一輸入完整 `FovResult` 逐欄相等；跨 zone movement 連跑兩次相等。 |

實際測試輸出的 FOV 除錯圖；`@` 東側緊貼一條牆，`.` 是可見格：

```text
    .
  .....
 .....
 ....
....@
 ....
 .....
  .....
    .
```

## 核心負向控制（真的紅）

暫時把遮蔽判定改為查目前格的 `GroundDef::move_cost`，完全不讀 `EdgeDef`；重新建置後執行：

```text
LocalFov.PassableTilesSeparatedByWallEdgeDoNotSeeAcross
Value of: visible(... {33, 32})
  Actual: true
Expected: false
fov_edge_negative_control_visible=49
1 FAILED TEST（exit 1）
```

也就是錯誤版把牆後格看見，可見格由正式版 **35 → 49**。之後已用 patch 恢復 `peek_edge` 版，
相同測試與全套測試重回綠燈。

## 被迫補上的機制

現有 `EdgeDef` 只有靜態種類，沒有每條門的開／關／鎖執行期狀態。為了不把易失玩法狀態塞進
程序 `LocalTiles`，本輪加了規範化 `LocalEdgeAddress` 與唯讀 `DoorStateQuery` 注入點；空 query
保守視為 Closed。FOV 與 movement 共用同一地址與門狀態語意。

未知鄰區採任務允許的直接「看不清」方案，而非尚未定案的 digest；`FovResult::degraded` 讓上層
能區分完整答案與退化答案。未撞到同層近距離事件快速路徑。

## 驗證

- `cmake --build build --parallel 2`：通過。
- 局部 `LocalFov.*:LocalMovement.*`：9/9 通過；每項都有非零路徑見證。
- `ctest --test-dir build --output-on-failure`：**249/249 通過**，79.44 秒。
- `./build/aetheria_sim --tick 62208000`：exit 0，輸出四層 zone tree。
- Godot fresh editor 首掃為已知 exit 139；原樣重跑 editor exit 0，主場景 exit 0。
- `git diff --check`：通過；未改 `design/`、`zone_manager.*`、`local_materialize.*`，未 push。

## 現有測試證不了的事

- 門狀態由未來的持久／易失狀態擁有者實際保存、開門動作扣時與發聲；本輪只提供唯讀判定接縫。
- 未載入鄰區 digest 的粗略視線品質；本輪依任務允許選擇不透明退化。
- Godot 畫面上的 FOV overlay；受「只動 core/local 新檔與測試」範圍限制，除錯圖由測試輸出，
  沒有改 M5.3 的 `gen local` viewer。
