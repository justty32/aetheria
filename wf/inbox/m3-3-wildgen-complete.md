# 信：M3.3 完成 — 荒野生成 W1～W6

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**任務來源**：使用者直接交辦 M3.3

## 完成內容

- 新增荒野專用的慢變數投影、`BoundaryProfile`、W1～W6 生成與 W5 快變數填充入口；既有
  `build_site_skeleton`／`populate` 簽章與城區 S1～S4、F1～F5 原始碼均未改。Site 首次展開與
  冷重算只在 `settlement == None` 時走荒野分支；聚落仍走原城區分支。
- W1 以四條共享剖面內插 64×64 高度，relief 控制噪聲與通行坡度；海域全水。固定 fixture 的
  平原／山地可通行格為 `2527 / 299`，山地確實仍是屏障。
- W2 河流從既定 crossing 以有界最低成本路徑接往另一 crossing；單入口時匯入湖泊。W3 道路
  以有界 A* 接 crossing，避水並在交會格形成橋。沒有「收斂為止」或無上界搜尋。
- W4 用資料驅動密度的抖動網格散布；測試逐 cell 證明最多一點，固定例疏植被 38 點、森林
  212 點。W5 只由既有快變數中的 owner 改遭遇密度，feature 改採集點、道路改旅人點；固定例
  `resources=3, encounters owned/unowned=2/6, travelers=3`。人口、建設、防禦、損毀改到極值時
  骨架不變。
- W6 產生 z=-1 的 L3 入口；一般荒野 1、山地 2、廢墟 4。Ruin 直接複用既有城區街廓演算法，
  再依 `site_wild.toml` 保留 20～40%；固定例保留 12 個結構。Sea 走全水、零陸上內容的便宜
  分支。所有坡度、密度、入口數與保留比例住資料檔。
- W4～W6 live 實體不登記 `AllComponents`，因此不存檔；冷重算會重新安裝。沒有新增 Region
  欄位、存檔欄位或 component 白名單項目。

設計中季節對採集點的修正、owner 控制力與鄰近衝突如何推導治安仍列在
`sitegen-wild.md`「待細化」，而既有 M2 `SiteFastVars` 也沒有這兩個輸入；本輪沒有捏造欄位或
偷改 M2 界面。W5 已把現有可表達的三條驅動路徑做成非空真例。

## 問題 1：邊界一致性接得上嗎？

**接得上。** 不需要 A Site 去問 B Site，也沒有新增 Region 欄位：兩側各自從既有 Region
慢變數，依規範 edge id／corner id 取得同一 seed，再只讀同一份剖面。垂直邊固定北→南，
水平邊固定西→東。

相鄰草原／沙漠、共用一條河的實例：

```text
wild_boundary_shared samples=64 crossings=1
  elevation_first=1500 elevation_last=1680
  bit_exact=elevation,ground,water_depth,edges,crossings
```

這不是只比輸入 profile：測試另從兩個**生成完成的實際 64×64 地形**抽回東／西邊，再逐欄
比較。五項判準全部有測：

1. 雙側逐位元一致（五欄且 crossing 非空）。
2. 先 B 後 A、先 A 後 B，各自完整 `WildernessSite` 相同。
3. A Site 從未生成時，B 的西邊仍等於 A 存在時的東邊。
4. 四個 Site 共用角的高度與 ground 相同。
5. 人工單調遞增剖面在東／西兩側都保持遞增，沒有鏡像。

測試曾真抓到一個錯：初版 profile 本身相同，但河道 A* 會沿邊界多刻一格，導致生成後的
ground／water 不一致。現在路徑除指定 crossing 外不得走邊界；修後實際地形五欄全等。

## 問題 2：荒野持久層真的近乎空嗎？

**玩法持久層是完全空的：0 個 `SitePersistentLayer` 物件。** 固定 live Site 實際生成：

```text
persistent_buildings=0 serialized_procedural_entities=0
live_vegetation=193 live_resources=3 live_encounters=6 live_travelers=3 live_portals=1
```

存檔後上列 206 個程序實體全部為 0；連續兩次冷展開之間刻意清空地形快取並刪掉程序實體，
第二次仍重建成相同計數：

```text
wild_rematerialize cold_runs=2 cache_corruption=1 recomputed=1
persistent_objects=0 procedural_entities=206
```

唯一仍會存的是所有 Zone 共用的 `ZoneMeta` 身分哨兵；它不是荒野玩法狀態。現在沒有任何荒野
物件「非存不可」。未來只有玩家挖通道路、開過寶箱、具名 NPC 等不可再生決定才應進持久層。

## 問題 3：30 ms 夠不夠寬？

30 ms **非常寬**。效能 fixture 同時啟用山地、廢墟、兩個河 crossing、兩個道路 crossing，
並讓 W1～W6 與廢墟街廓複用全部真跑；暖機後連跑 8 個 seed 取 worst：

```text
Debug worst   4.10808 ms
Release worst 0.674469 ms
timed_runs=8 W1_tiles=4096 W2_river_paths=1 W3_road_paths=1
W4_vegetation=5 W5_resources=3 W5_encounters=6 W5_travelers=3
W6_portals=4 ruin_structures=9
```

Release 尚有約 44 倍餘裕。我的建議是：**30 ms 留作絕對 fail 門檻，但荒野另設 5 ms Release
回歸預算**；它仍給待細化內容約 7 倍空間，也更符合「會大量生成」的風險。若未來會同一幀
prefetch 多個 Site，還應再加批次總預算，不能因每個都低於 30 ms 就一次生成很多個。

## 完整驗證

```text
cmake --build build --parallel 2                    PASS（四 target，零警告）
ctest --test-dir build --output-on-failure          189/189 PASS
./build/aetheria_sim --tick 62208000                exit 0
godot-mono --headless --path godot --editor ...     exit 0
godot-mono --headless --path godot --quit-after 5   exit 0
CoreIsolation.CompileCommands                       PASS（core 零 godot-cpp）
cmake --build build-release --target aetheria_tests --parallel 2  PASS（零警告）
Release Wilderness*                                 13/13 PASS
```

沒有做城建循環、沒有改城區生成、沒有改 M1 管線或 M2 既有界面、沒有新增 Region 欄位、沒有
fan-out 子 agent，也沒有 push。
