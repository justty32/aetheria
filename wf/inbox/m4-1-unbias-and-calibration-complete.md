# 信：M4.1 完成 — 無偏方向、真實校準與 min-of-5 效能斷言

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**任務來源**：使用者直接交辦 M4.1

## 完成內容

- 所有現有 wall-clock 效能量測統一為：暖機一次，固定量 5 次，取最小值；沒有任何
  「重試到過」。9 個時間門檻（8 個測試）全部改完，只有診斷輸出、沒有門檻的城建 20 旬、
  城市選址、勢力擴散、河流尺度也一併改成 min-of-5。規則已寫入 `wf/workflows/testing.md`，
  共用 helper 是 `tests/support/performance.h`。
- 新增固定種子的 100 條隨機 live／absent 序列，每條固定 20 旬；逐列輸出 mean、標準差、
  相對偏差與方向，並用人工 +1% 正偏控制證明方向檢查會抓到可刷漏洞。
- 測試先抓到 M4.0 重載會清掉 digest 已保存的 `population_micro_remainder`；修成保留後加了
  直接回歸測試。這是生命週期狀態損失修正，沒有調玩法參數。
- M2.4 的零誤差空測試已換成 1000 個隨機 Region 初態：真正跑一旬逐小時 Site 公式，再與
  同一初態的一次 Region 近似公式比較四列歸約量。本輪沒有做 M4.2、命運無偏或 M1～M3
  機制改造。

## 1. 無偏性有沒有方向？能不能刷？

有方向，但方向是**偏低**，不是偏高；因此不能靠反覆進出城刷資源。

```text
row          L_FULL baseline   random mean   stddev    bias        direction
population   233               229.99        1.05352  -1.29185%   low
development  1                 1             0         0%          neutral
food         14400             14400         0         0%          neutral
production   28800             28800         0         0%          neutral

sequences=100  xun_each=20  live_xun=989  absent_xun=1011  transitions=1029
positive_control_bias=+1%  detected_direction=high
```

四列偏差都小於 5%，且方向斷言明確拒絕 `mean > baseline`；不是只有絕對差異小。修正人口小數
餘額遺失前，人口曾是 mean 226.73、stddev 1.84312、bias -2.69099%；測試確實抓到反覆切換
特有的狀態損失，修正後降為上表的 -1.29185%。剩餘人口偏低來自既有 Region 近似公式本身
略低於逐小時公式，沒有正向套利。

## 2. 真實校準誤差

每個樣本隨機人口 25～450、Village／Town／City、資源存量、owner、relief、feature、elevation
與 Site seed；從同一狀態各推進一旬。每個樣本取人口／建設／糧食／生產四列相對誤差的最大值：

```text
samples=1000  nonzero=603
min=0%
median=0.473934%
p95=1.73536%
max=2.63158%
over_5pct=0/1000 (0%)
```

所以這次第一次量到真實非零誤差，且全部低於 5%；603 個樣本非零也防止退回 M2.4 的
「同公式跟自己比」空測試。沒有為了門檻調任何參數。

## 3. 還有哪些測試對機器狀態敏感？

靜態掃描所有 `steady_clock`、時間門檻、CTest timeout、sleep/wait 後：**沒有剩餘的單次或
平均效能斷言，也沒有其他會因機器忙而紅燈的時間上限測試。** 兩個測試 support 仍用
`steady_clock` 產生暫存路徑名稱，但不比較耗時；與機器負載無關。

所有 wall-clock 診斷現在也都是固定 min-of-5。它們的顯示值仍可能在「連續 5 次全都受競爭」
時一起被抬高；這是 wall-clock 量測無法完全消除的物理敏感性，但已沒有單樣本／平均污染，
且固定 5 次保留確定的執行時間上界。代表性實測：

```text
                                Debug min-of-5   Release min-of-5   budget
荒野 W1～W6                    3.0948 ms        0.461332 ms        30 ms
城區 skeleton + F1～F5         5.4345 ms        0.927654 ms        30 ms
Region 十二階段                522.025 ms       45.3257 ms         3000 ms
```

## 完整驗證

```text
cmake --build build --parallel 2                  PASS（全 target，零警告）
ctest --test-dir build --output-on-failure        199/199 PASS
./build/aetheria_sim --tick 62208000              exit 0
godot-mono --headless --path godot --editor ...   exit 0
godot-mono --headless --path godot --quit-after 5 exit 0
CoreIsolation.CompileCommands                     PASS（core 零 godot-cpp）
cmake --build build-release --target aetheria_tests --parallel 2  PASS（零警告）
Release M4.1 + 全效能關鍵案例                    15/15 PASS
```

本輪沒有 fan-out 子 agent，也沒有 push。
