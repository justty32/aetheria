# M5.15 Local → Site 歸約完成回報

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者

## 落地內容

- 把 L2→L1 原有的 typed row storage、封閉 delta、絕對快照 apply 抽成
  `core/spatial/reduction.h`；L2→L1 與 L3→L2 各保留自己的固定 row 清單與唯一
  `ReductionTable` writer。delta 建構子、values 與 apply 仍是 private，不能由一般呼叫端拼寫。
- L3→L2 只有設計表上的四列：建築完整度、控制方、資源產出修正、通行成本；沒有加第五列。
- `LocalPayload::reduction` 收四類 Local 觀測：結構段、控制點、採集點、通道。
  `SiteLayers::local_reductions` 是 4096 個 Site tile 的私有四列 storage，公開只讀。
- `reduce_live_local` 驗證 Local key、父格座標與 LOD，再以同一套絕對快照機制寫父 Site tile。

本輪量測定義：完整度＝未損毀結構段比例；控制方＝控制點多數、平手為無主；資源修正＝
總剩餘量／總容量百分比；通行成本＝現存通道最低成本。這四條是現有設計表未給公式時的
最小整數實作，沒有寫回 `design/`；控制點平手與成本聚合仍請規劃者裁定。

## 驗收實測

| 判準 | 實測 |
|---|---|
| 非空 Local 真有內容 | 結構段 4（損毀 1）、控制點 3、採集點 2、通道 2；四列寫入數 4 |
| 四列正確 | 完整度 `75`、控制方 `FactionId{2}`、資源修正 `75`、通行成本 `150` |
| 複用而非複製 | 共用 `Snapshot::apply_to → apply_absolute`；下節故障注入使兩層同時紅 |
| 不算兩次 | 通行成本 `200→175→150`，把最後快照再套一次仍 `150`；若把同一個 `25` 再扣一次會是 `125` |
| 空 Local | 四列 delta 均為 `nullopt`、`writes=0`；父層保留 `75,2,75,150`，沒有寫成 0／預設值 |
| 決定性 | 正序、重跑、四類觀測反序皆為 `7765383609656274199`；通行成本 `150→149` 後為 `7762513884307211164` |
| 結構性摩擦 | `LocalTileDelta` 不可 default construct；Site 四列 storage private，friend 只有 Local `ReductionTable` |

## 共用機制負向控制（真的注入、真的紅、已還原）

暫時把 `core/spatial/reduction.h` 唯一 apply 從「寫 snapshot 值」改成「寫 row 的
0/default」，以相同二進位跑既有 L2 測試與新 L3 測試。exit code **1**，兩條同時紅：

```text
LocalReduction.NonEmptyLocalMeasuresExactlyFourRowsAndAppliesToSiteTile
  integrity expected 75 actual 0
  controller expected 2 actual 0
  resource_modifier expected 75 actual 0
  passability_cost expected 150 actual 0

SiteReduction.FixedRowsAreTheOnlySiteSideWriter
  population expected 100 actual 0（後段 expected 75 actual 0）
  development_level expected 1 actual 0

2 FAILED TESTS
```

還原後同一 filter 為 `6/6 PASS`。因此不是兩份複製的 apply，也不是未執行的假注入。

## 被迫新增的機制／對抽象的反對票

**一項：per-row absence。** 原 L2→L1 delta 是每列必有值的完整快照，無法表達任務要求的
「空 Local＝沒有變化」；若沿用原形狀只能把空層量成 0，會清掉父 Site 既有值。因此共用
snapshot 的每列改成 `std::optional<Value>`：present 才絕對覆寫，absent 不寫。

這對應的抽象不足是：L1↔L2 原機制只描述「完整 child snapshot」，沒有描述「child 對某列
沒有觀測」。這是 L2↔L3 迫使機制補出的語意，依任務書標準應算一張**反對票**，不是單純新增
四列 schema。既有 Site→Region reducer 仍固定填滿四列，行為未變；故障注入證明 apply 真共用。

除此之外沒有新增事件通道、累加帳本、第二套 hash、第二套 apply 或新的生命週期機制；四類
觀測與 Site storage 是尺度降一級所需 schema。

## 現有測試證不了的事

- 本分支依規約不能碰 `zone_manager.*`／`local_materialize.*`；測試證明同一入口可歸約，**尚未證明**
  M5.13 合併後每旬與卸載路徑一定會自動呼叫它。
- Local codec 目前刻意不存程序 payload；本輪未升存檔格式。測試不證明四類 Local 觀測或
  Site 四列跨冷卸載持久，需由後續持久層／生命週期裁定。
- 測試用非空 `LocalPayload::reduction` 真有上述計數，但尚無採集、破壞、控制點玩法命令自動
  維護這些觀測；不能據此宣稱 gameplay producer 已完成。
- 多勢力平手、資源百分比與最低通道成本只是最小整數規則；未有設計校準資料，測試只證明
  實作決定性與界面封閉，不證明玩法平衡或 contested 語意正確。

## 完整驗證

```text
cmake --build build --parallel 2                  PASS（四 target，零警告）
ctest --test-dir build --output-on-failure        245/245 PASS，80.97 s
./build/aetheria_sim --tick 62208000              PASS
godot editor 首掃                                exit 139（工作流已知首掃）
godot editor 原樣第二次                          exit 0
godot 主場景                                     exit 0
CoreIsolation.CompileCommands                     PASS
```

未修改 `design/`、禁改的四組並行檔或 `zone_manager.*`，未 push。
