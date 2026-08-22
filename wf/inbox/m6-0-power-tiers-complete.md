# M6.0 完成回報 — 力量體系與等效戰力

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

**原任務**：[m6-0-power-tiers.md](done/m6-0-power-tiers.md)

## 結論

已落地資料驅動五階權重、quality 值域、百分點定點 `S`、cohort tier、個體階差門檻與
破階 def。戰鬥位階直接使用 `world::Significance`；沒有新增 tier enum。乘加溢位採
`AETH_CHECK` fail-fast，death test 已命中；沒有 `std::pow`、`double` 或浮點運算。

內部 `S` 保留百分點比例，是顯示 S 的 100 倍：這避免 tier 0、quality 50% 的單位被整數除法
截成 0。只要比較雙方，比例不受影響。

## 驗收實測

| 判準 | 結果 |
|---|---|
| 兩件事同時成立 | 固定案例 **N=100**：tier 4 為 25,600、100×tier 0 為 10,000，tier 4 勝；**M=1000**：1000×tier 0 為 100,000，tier 4 敗。顯示 S 分別為 256、100、1000。 |
| 精確切點 | 同為 100% quality 時，255 人 25,500（敗）、256 人 25,600（平）、257 人 25,700（勝）。固定 N/M 有 900 人的明顯案例區間，但線性比較的數學邊界仍是相鄰三點；沒有隱藏。 |
| 堆人數不升階 | 1000×tier 0：`cohort_tier=Ambient`（tier 0），只有 S 增至 100,000。 |
| 階差門檻 | 10,000×tier 0 → tier 4 個體：1,000,000→0；10,000×tier 1 → tier 4：4,000,000→0；10,000×tier 2 → tier 4：16,000,000→16,000,000。 |
| 破階手段 | 同一組 tier 0→tier 4 輸入，加 `breakthrough.artifact` def 後 0→1,000,000，完整恢復。 |
| 門檻只管個體 | tier 0→tier 4 cohort 輸入不套門檻，維持 1,000,000。 |
| 決定性 | 同一混合 cohort 重算 100 次逐次相等；全整數。 |
| 數值資料化 | 權重、quality 最小／基準／最大、門檻 gap 與傷害比例都只在 `data/power.toml`；C++ 沒有權重常值表。 |

## 五階命中計數

五階基準矩陣逐階呼叫真正的 `equivalent_power`，命中計數為：

| tier | Ambient | Local | Site | Region | World |
|---|---:|---:|---:|---:|---:|
| 命中 | 1 | 1 | 1 | 1 | 1 |

這不是只檢查資料列存在；每一階都實際取權重並算出 S。其他 N/M、quality 與門檻案例的額外
命中未灌進這個計數，避免把同一案例重複算得好看。

## quality 貼齊

四組相鄰階實測全部恰好相等：200=200、800=800、3200=3200、12800=12800（內部 S）。
也就是頂裝 tier N 與劣裝 tier N+1 毫無餘裕。

我的判斷是**目前可接受的貼齊**：quality 本身仍沒有跨階，只能追平，而且真正的質差仍由
Significance 與個體門檻保留。不過下輪若再把 terrain／morale 等修正混入同一個 quality，
就很容易實質跨階；那些修正應留在戰鬥公式自己的乘項。若設計要求「任何量化比較都嚴格
小於下一階」，才需要由規劃者收窄值域，本輪未自行改值。

## 負向控制（真的紅）

精確故障注入只把 `damage_numerator` 暫由 0 改成 1（分母仍為 1），等效拿掉個體門檻：

- `PowerTierGate.ProtectsOnlyIndividualsAndBreakthroughRestoresDamage` exit 1。
- tier 0→tier 4：預期 0、實際 **1,000,000**（古龍掉 10,000 顯示 S）。
- tier 1→tier 4：預期 0、實際 **4,000,000**（掉 40,000 顯示 S）。
- tier 2、cohort、破階分支沒有因注入而假紅。資料已還原，測試回綠。

第一次探索注入曾反轉 `defender_is_individual`，雖同樣讓前兩項變成 1,000,000 而紅，卻也
錯誤壓掉 cohort，總共紅三個斷言；因控制不夠單一而不採作主證據。沒有試調任何正式權重或
quality 值；N/M 只測任務指定語意的 100／1000，再由目前資料推導 255／256／257 邊界。

## 驗證

- `cmake --build build --parallel 2`：成功（fresh build 1290/1290；最終增量成功）。
- `ctest --test-dir build --output-on-failure`：**278/278**。
- 力量專屬：6/6。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- Godot editor 首掃完成後 exit 139（既知首掃情況）；原樣重跑 exit 0。
- Godot 主場景 headless：exit 0。

## 現有測試證不了的事

- 本輪只有 S 比較與「損傷分配給個體」的門檻，尚未有 Region 主公式、潰散、逼退或生命值；
  所以 N/M 證明的是戰力比較，不是完整戰鬥模擬的實際勝負。這屬下一輪範圍。
- `cohort_tier` 目前取非空 stack 的最高位階；「該位階者是否能指揮」尚無指揮關係輸入，
  現有測試不能證明這個例外條件。
- 破階 def 已走 Ruleset 並能解除門檻，但神器／法術／陷阱如何取得與消耗尚未設計、未測。

未修改 `design/` 或任務列出的並行禁區，未 push。
