# M5.19 完成回報 — 路線 C：負 z 地下結構

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

**對應任務**：[m5-19-route-c-underground.md](done/m5-19-route-c-underground.md)

## 結果

已完成三種純 C++ 路線 C：礦坑沿 seed 決定的礦脈方向掘進並生成 12／10／8／6／4 格
遞減分支；地城以房間及直線＋一次轉折走廊連成；遺跡先呼叫既有路線 A，再按 seed
拆除 60～80% 實體牆段。三者地表都走路線 B，負 z 層仍落在同一個
`LocalPayload::layers`，沒有另建 zone、portal 或跨 zone 傳送。

Site 的 `structure` def 新增地下種類與 1～16 層深度欄位。固定資料分別是礦坑 2 層、
地城 3 層、遺跡 1 層；特殊 def 標成 landmark，不會混入一般城市建築配額。

## 驗收實測

Debug build；固定測試輸入使用 `kLocalSiteSeed=0xA4217B3E9C60D5F8` 與其正式
`derive_local_seed`。

| 判準 | 實測結果 |
|---|---|
| 可達性 | 礦坑、地城、遺跡的所有挖開格皆可由 z=−1 入口抵達；地城 27 房、遺跡 60 房，`unreachable_rooms=0` |
| 深度受控 | mine/dungeon/ruin = **2/3/1 層**，對應 layers = 3/4/2（含 z=0）；未默認十層 |
| 遺跡拆除 | 原始實體牆段 1,299，拆 988，**76.0585%** |
| 10 ms 預算 | 暖機一次後固定 min-of-5：**2.97943 ms**；該次挖 2,234 格、27 房、24 段走廊、3 個負 z 層 |
| 決定性 | 同 seed 正規化雜湊皆為 `13040317289428397378`；seed + 1 後不同 |
| 垂直連結 | 3 筆相鄰層 link；每筆 `lower_z=upper_z−1`，同一格上下兩端皆為 Stairs，雙向一致 |

生成不使用 A*、回溯或鬆弛；可達性檢查才使用跨負 z 層 flood fill。正式生成結束也會跑
同一組不變式，不能把不可達結果靜默交給呼叫端。

## 負向控制（真的紅）

固定地城有 **27 房**。測試只把最後一個房間唯一入口走廊的目的門檻改成實牆，正式驗證
得到 `sealed_corridors=1`、`unreachable_rooms=1`、`invariant_rejected=1`。

為確認注入真的紅，暫時把故障後期望值從 1 反改成 0，重新建置並單跑測試；gtest 實際輸出：

```text
LocalUnderground.SealedCorridorNegativeControlFindsUnreachableRoom
Expected equality of these values:
  unreachable
    Which is: 1
  0U
    Which is: 0
rooms=27 sealed_corridors=1 unreachable_rooms=1
1 FAILED TEST
```

已恢復正式斷言 `EXPECT_EQ(unreachable, 1U)`，同測試與全套 CTest 均回綠。

## 除錯圖

指令：

```sh
./build/aetheria_sim gen local --site-seed 0x5A17 --zoning dungeon --z -1 \
  --output out/m5-19/dungeon
```

實際輸出為 depth=3、挖 2,233 格、27 房、24 走廊、3 垂直連結。已目視檢查
[z=−1 房間／走廊圖](../../out/m5-19/dungeon/local-dungeon-zm1-rooms.png)：513×513、8-bit
RGB PNG；SHA-256
`b6794439816f568d1af43b867473c652d013ff62e4a5f53fc373b1949e03cfae`。

## 新機制與抽象缺口

1. 既有 `LocalSlowVars::structure` 只有 `BuildingDefId`，def 沒有路線 C 種類或深度，無法
   兌現「深度由 structure 決定」。因此在既有 structure def 補 `UndergroundKind` 與
   `underground_depth`；慢／快分流本身成立，缺的是 schema 欄位。
2. 既有 `z → LocalTiles` 與同格 Stairs 足以表達所有垂直層，沒有需要新垂直機制，表示
   M5.2 的同-zone layers 抽象可沿用。
3. Site 慢輸入沒有顯式礦脈向量；本輪以 structure/local seed 決定穩定礦脈方向。若未來
   要讓上層地質場控制實際走向，仍需補一個慢欄位，不應偷讀快變數。

## 現有測試證不了／排除項

- 現有 schema 沒有劇情深度 override，測試只能證明 **structure def** 控深；劇情如何改深度
  仍需規劃者另定載體。
- 2.97943 ms 是本機 Debug 固定資料，不代表所有硬體、Ruleset 與最佳化建置上界。
- 本輪未做機關、寶藏、`cleared` 防刷、光照壓力或其他地城玩法。

## 驗證

- `cmake --build build --parallel 2`：通過，零警告。
- `ctest --test-dir build --output-on-failure`：**264/264** 通過，82.71 秒。
- `./build/aetheria_sim --tick 62208000`：exit 0，抵達 year 3 / season 1 / month 1 / xun 1。
- Godot 4.7.1 headless editor 與主場景：均 exit 0。
- `SimViewers.GeneratePng` 含地下 z=−1 固定見證：通過；`git diff --check`：通過。

未修改 `design/`、`core/local/local_path.*`、`local_fov.*`、`local_reduction.*`，未 push。
