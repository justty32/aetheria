# M6.1 個體規則完成回報

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

**對應任務**：`m6-1-individual-rules.md`

## 完成內容

- `attributes.*`：固定四屬性 `Body／Skill／Mind／Spirit`、九項即時計算衍生值、
  `suggest_tier` 與必須附理由的位階覆寫。
- `check.*`：注入 `std::mt19937_64` 的單骰 d100；成功、餘量、效果段都來自同一骰。
- `damage.*`：`DamageTypeDef`＋強型別下標、稀疏每型別抗性、負抗性與 90% 上限。
- `attributes.toml`、`damage.toml` 已接入不可變 `Ruleset`；載入期驗證值域、嚴格遞增
  門檻／分段、重複 id、傷害型別前綴與抗性上限。
- 未實作階差門檻、狀態效果、兵種相剋；未修改 `design/`、M6.0／M6.2 檔案或
  `significance.h`。

## 驗收實測

| 判準 | 結果 |
|---|---|
| 一次擲骰 | seed `0xD100C0DE` 跑 10,000 次：檢定 10,000、RNG 原始呼叫 10,000；兩邊下一值同為 `3581667339757956145` |
| 同 seed 同結果 | 10,000 筆 `CheckResult`（roll／target／margin／success／band／effect）逐筆相同 |
| 分段有效 | 最低效果 `0%`、最高效果 `150%`；兩者不同 |
| 抗性可負 | 基礎傷害 100、抗性 −25% → 實際傷害 125（原值 100） |
| 不可完全免疫 | 輸入抗性 1,000,000% → 套用 90%；100 傷害 → 10，減傷 90% `<100%`；1 傷害仍為 1 |
| `suggest_tier` | 隨機 1,000 組：超差組數 0、最大差 0；預設實際 tier 即建議值 |
| 決定性／整數 | 新規則來源搜尋 `double`、`std::pow`、內建 seed／時鐘皆 0 筆；公式使用整數 |
| def 非 enum | `static_assert(!std::is_enum_v<DamageTypeDef>)` 通過 |
| 只改資料加型別 | 測試只在暫存 `damage.toml` 追加 `damage.arcane`，冷載入由 9 種變 10 種且可查得 |

九項衍生值參考案例（屬性 40／50／60／70、Site 位階、裝備修正 1～9）皆真的計算：
生命 311、魔力 408、命中 61、迴避 58、防禦 50、抗性 67、移動 13、負重 102、視野 15。

## 覆蓋命中計數

同一批 10,000 次 d100 的餘量段：

| failure 0% | success 100% | strong 125% | exceptional 150% |
|---:|---:|---:|---:|
| 3,027 | 1,882 | 1,981 | 3,110 |

傷害型別參考案例每種各命中 1 次：

| slash | pierce | blunt | fire | ice | lightning | poison | dark | holy |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |

`suggest_tier` 五階分布（同一組固定 seed、每屬性 0～100）：

| Ambient | Local | Site | Region | World |
|---:|---:|---:|---:|---:|
| 168 | 238 | 249 | 202 | 143 |

## 負向控制（實際故障注入，皆已還原）

1. **餘量改成另擲暴擊骰**：`IndividualCheck.OneInjectedRngCallPerCheckIsDeterministicAndEveryBandIsHit`
   exit 1。相同 seed 的單骰期望／故障實際從索引 1 起為 `37→41、41→36、87→52、
   36→78、50→60、52→66、37→77`；10,000 次後 RNG 下一值由期望
   `3581667339757956145` 變成 `2622961125510241170`。故障版分段命中也由
   `3027/1882/1981/3110` 變成 `2515/2377/2572/2536`。
2. **拿掉抗性上限**：`IndividualDamage.ExtremeResistanceIsCappedBelowCompleteImmunity`
   exit 1。套用抗性由期望 90 變 1,000,000；100 基礎傷害由期望 10 變 1。

## 參數嘗試紀錄

- 位階平均屬性門檻只選一次 `[35,45,55,65]`，直接得到上述五階分布，未重試調數字。
- 衍生公式與四段效果採第一版資料值，未反覆試值。曾把視野參考案例手算期望誤寫 18；
  實際公式是 `3 + (70 + 2) / 20 + 9 = 15`，只修測試期望，沒有改公式或資料。

## 驗證

- `cmake --build build --parallel 2`：通過。
- `ctest --test-dir build --output-on-failure`：280/280 通過（最終 80.12 秒）。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- Godot editor：全新 worktree 首掃完成後重現已知 exit 139；原樣第二次 exit 0。
- Godot 主場景 headless：exit 0。

## 現有測試證不了的事

- 這輪證明公式接線、覆蓋與決定性，不證明長期成長、裝備經濟或戰鬥手感已平衡。
- `rng() % 100` 才能結構性保證一次原始 RNG 呼叫；因 `2^64` 不能被 100 整除，
  全狀態空間有極微小 modulo bias。若要求數學上完全均勻，需規劃者裁定如何與
  「每檢定恰一次 RNG 呼叫」同時滿足。
- 尚無 L3 戰鬥實體／狀態效果消費端，因此只驗證規則核心與 Ruleset，不證明完整戰鬥流程。
