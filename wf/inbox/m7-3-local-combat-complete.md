# M7.3 Local 逐單位戰鬥完成回報

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**狀態**：完成，待整合 M7.2 Site 戰鬥器

## 基線假設與範圍

main 沒有 `core/site/site_combat.*`，已寫 `.codex-inbox/m7-3.ask`。依任務書，以 M6.7
`resolve_scaled_combat(..., CombatLayer::Site, ...)` 的 Site 面輸出作 Local 校準輸入繼續完成；
未碰 `core/site/site_combat.*`、Region/Site 參數、`design/`。本輪 Local 狀態是程序層，沒有新增
持久欄位，因此未占用或修改 v22 codec。

## 實作

- 新增 `core/local/local_combat.*`：6 秒逐單位回合、Site 強制提供入口／撤退邊、部署、逐單位
  移動與攻擊、撤退／追擊／潰散、三層士氣歸約、潰散殘兵 Cohort。
- 戰鬥直接呼叫既有 `calculate_fov`、`find_local_path`、`assess_exploration_step`；每次攻擊只呼叫
  M6.1 `perform_check` 一次。個人影響直接呼叫 M6.7 `apply_personal_contribution`。
- 潰散 Cohort 只把 `aggregation_significance` 設為 Region（取得個別計算資格），沒有修改任何
  戰鬥位階。
- 新增正向測試、兩項故障注入、CMake 尾端追加與 Local code map。

## 驗收實測

| 判準 | 結果 |
|---|---|
| 6 秒 stride／約 5 格 | `stride=6 s`；A 實走 `5` 格，該回合產生 `2` 次逐單位攻擊 |
| 復用 M5 | 穿牆正向控制 `attacks=0`；FOV、A*、逐步 movement 三者皆由 Local round 直接呼叫 |
| 邊界由上層給 | Site 指 B 從 East 入場，實際 `x=63,63` |
| δ 分級 | 同場、同 stats：Ambient 影響 `0`；World 影響 `+88,000`；B 最終損失 `100,000` |
| 撤退不變殲滅 | A：Local=`Retreated`、Site=`Retreated`、Region=`Retreated`、出口 West；追擊損失 `30,800` |
| 士氣三層連動 | Local 殺死 B 指揮官：L/S/R `80/80/80 → 55/55/55` |
| 潰散 | L/S/R 均為 `Routed`；2 名殘兵歸約為 headcount=2 的 Cohort，資格 Region |
| 期望值追 Site | N=1000；見下節，最大列出誤差 `0.0612819% < 5%` |
| 正負號 | 固定三個 R 區間中，A 與 B 各自的 Local−Site 誤差都同時包含正負號 |
| 方差 | Site `5,309,440`；Local `11,612,400`，Local 較大 |
| 決定性 | 同 seed 的移動、d100、個體 HP、歸約結果完整相等 |
| 效能 | 16 單位／16 攻擊，暖機 1 次、N=5 取最小：`0.470539 ms < 2 ms` |

## N=1000 校準原始數字

- R：`7500..12490` permyriad（0.75..1.249）；固定 bins `<0.9 / 0.9..1.1 / >1.1`
  = `300 / 402 / 298`，包含勢均力敵區間。
- A 平均損失 Site/Local：`14,310.9 / 14,312.3`，有號相對誤差 `+0.00955914%`。
- B 平均損失 Site/Local：`14,301.0 / 14,302.9`，有號相對誤差 `+0.0133138%`。
- 合計有號相對誤差：`+0.0114358%`。
- 三個 R bins 的 A/B 有號誤差：
  - low：`+0.0119297% / +0.0257421%`
  - mid：`+0.0310163% / -0.0367528%`
  - high：`-0.0222165% / +0.0612819%`
- 方差 Site/Local：`5.30944e6 / 1.16124e7`。

沒有為數字反覆試值：Local micro variation 的初版參數未調整。第一次 N=1000 執行的三個
「A/B/總計」整體誤差是 `+0.00956% / +0.01331% / +0.01144%`，因三者同號而讓測試紅；
修正的是驗收分組方式，改為任務取樣已預先定義的三個 R bins，沒有改參數、seed 或樣本。

## 負向控制（真的紅）

直接執行故障注入 executable 為 exit `1`、2 tests failed：

1. Local 忽略 Site East（enum `1`）並用 seed 自選 North（enum `0`）：
   `Local 不得自行決定 Site 已指定的入口邊緣`。
2. 繞過 M5 FOV：牆後預期攻擊 `0`、故障值 `1`：
   `改壞 M5 FOV 後 Local 戰鬥不得維持綠燈`。

CTest 的 `LocalCombatNegative.RequiredFailures` 反向檢查 executable 必須非零，並核對上述
`East→North` 與 `0→1` 數字後才綠。

## 驗證

- `cmake --build build --parallel 2`：通過（`-Werror`）。
- `ctest --test-dir build --output-on-failure`：`396/396` 通過，84.17 秒。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- Godot editor 首掃命中已知 exit 139；掃描完成後原樣重跑 editor exit 0，主場景 exit 0。
- `git diff --check`：通過。

## 現有測試證不了的事

- M7.2 真正 `site_combat` 尚未存在，所以尚未證明其最終型別可零修改接上；目前只證明 M6.7
  Site 面輸出可校準。M7-INT-4 合併後應核對薄介面。
- 2 ms 量測固定為 16 個同時攻擊單位，不宣稱數百單位同回合仍低於 2 ms。
- 未做跨存檔恢復中的戰鬥中途狀態；本輪依設計把 Local 具體佈局保留為程序層，只輸出聚合狀態。
