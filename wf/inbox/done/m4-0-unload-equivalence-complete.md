# 信：M4-0 完成 — Site 卸載等價、閉式補算與飽和

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**任務來源**：使用者直接交辦 M4-0

## 完成內容

- 新增可存檔 `SiteDigest`：保存 `unload_tick`、`site_seed`、`skeleton_hash`、持久物件、
  已完成／未完成城建、經濟續接值與劇情旗標；卸載時移除 live `CityBuildState` 並清空 payload
  持久層，磁碟只留 digest。程序層／易失層仍由既有白名單結構性排除。
- 新增時鐘式 `unload_site_zone` 與帶 `now` 的 `rematerialize_site_zone` overload。重載依序重建
  骨架、用當下 Region 快變數填充，再疊回 digest 並以 `now - unload_tick` 一次閉式補算。
  骨架 hash 不符會明確拒絕；本輪按指示不做遷移。
- pending 以 elapsed 小時一次扣減，完成只轉成一棟建築一次；人口、糧食、生產直接接當下
  Region 權威值。Region 缺席近似同步推進完整城建基準的糧食／生產，人口封頂於目前住宅容量
  500，避免長時間缺席與 L_FULL 容量規則分岔。
- 持久建築狀態依當下 owner／damage 重算；不利環境下老化每 6 旬降一級，elapsed 以 24 旬
  封頂。L_FULL 小步路徑與 L_ABSENT 閉式路徑共用同一個持久物件轉換函式。
- 存檔格式 v12 → v13；`SiteDigest` 依規矩加在 `AllComponents` 尾端，版本沿革已補。

沒有做命運無偏、骨架遷移、隨機載入序列，也沒有做 M5 多 Site 批次推進。

## 1. A／B 實際差異

兩條路徑都從 Tick 0 出發，以目標時鐘 `N × 864,000` 秒驅動；A 換算成逐小時 L_FULL
推進，B 以 Region 旬 stride 走到同一 Tick 後冷重載。相對誤差定義為
`abs(A-B) / max(A,B,1)`，表中誤差是四列歸約量的最大值。

```text
N    A L_FULL (pop/dev/food/prod)    B L_ABSENT→reload (pop/dev/food/prod)    max error
1    104 / 1 /   720 /   1440        104 / 1 /   720 /   1440                  0.0000%
5    123 / 1 /  3600 /   7200        123 / 1 /  3600 /   7200                  0.0000%
20   233 / 1 / 14400 /  28800        229 / 1 / 14400 /  28800                  1.7167%
100  500 / 1 / 72000 / 144000        500 / 1 / 72000 / 144000                  0.0000%
```

四個 N 的持久建築、已完成城建與 pending 集合都相同。每次 B 補算都同時輸出
`pending_advanced=1 completed=1`：fixture 有一棟尚餘 48 小時、且不影響經濟校準值的廣場，
所以補算不是空跑。N=20 的兩條人口公式確實產生 4 人差距，不是全程共用同一終值公式。

## 2. 負向控制有沒有超過 10%？

**有。** N=20 保留冷載的 digest，但故意完全跳過第三步疊回／補算，再做歸約：

```text
A L_FULL population=233
skip catch-up population=0
max_relative_error=100%
```

這條也斷言冷載時 `SiteDigest=1`、`CityBuildState=0`，因此不是沿用記憶體中的 live 狀態。
100% 明確超過 10%，卸載等價測試具偵測力。

## 3. 飽和上限有沒有被打到？

**有。** 老化 cap 是 24 旬；N=24 剛好等於 cap，從 **N=25** 起 `min(elapsed, cap)` 會實際
截斷。另以 N=10,000 旬冷重載驗證：

```text
persistent_objects_advanced=1
aging_transitions=3          # Active → Idle → Derelict → Ruined
aging_seconds_applied=24 xun
cap_hit=1
pending_advanced=1 completed=1
```

所以這條目前不是空規則；離開 10,000 旬也只會把該建築推到 Ruined 一次，不會線性傾頹。
本輪沒有隨機事件系統，因此沒有虛構事件抽樣計數；未來若新增 elapsed 驅動事件，仍須另設
設計指定的 6 旬 cap。

## 完整驗證

```text
cmake --build build --parallel 2                  PASS（全 target，零警告）
ctest --test-dir build --output-on-failure        197/197 PASS
./build/aetheria_sim --tick 62208000              exit 0
godot-mono --headless --path godot --editor ...   exit 0
godot-mono --headless --path godot --quit-after 5 exit 0
CoreIsolation.CompileCommands                     PASS（core 零 godot-cpp）
```

本輪沒有 fan-out 子 agent，也沒有 push。
