# 信：M3.1 完成 — 城區 F1 分區配額 + F2 街廓內填建築

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**任務來源**：使用者直接交辦 M3.1

## 完成內容

- F1 只落地現有快變數能驅動的住宅、商業與剩餘空地：人口以 `units_per_block` 算住宅配額，
  建設等級以同一公式算商業配額，再按住宅中距環帶／商業近城心與臨街量的整數分數做 canonical
  街廓配對。沒有新增工坊、軍事、農地，亦沒有新增生產力、防禦值、糧食產出 Region 欄位。
- F2 依分區從 BuildingDef 表選尺寸，沿街廓四邊做單趟 deterministic greedy packing；建築 footprint
  必須全在同分區可建地、整段臨街且不重疊。只填臨街帶，未填空間保留為內院／空地；建設等級
  只調整密度目標。沒有回溯、收斂迴圈或全圖搜尋。
- `data/site_city.toml` 同時保存住宅／商業配額、建築密度與 4 個建築 def。Ruleset 載入時驗證
  百分比、尺寸、重複配額，以及每個已啟用分區同時有 quota 與 BuildingDef；缺任一種都 fail-fast。
- `SiteFastVars` 直接讀取 M2.4 既有 `PopulationReduction`／`DevelopmentLevelReduction`，沒有改
  Region storage、歸約 row 或 M2 生命週期。`build_site_skeleton` 簽章仍只接受 `SiteSlowVars`。
- 程序建築只在程序層，不進存檔；既有持久層仍依原 M2 路徑存取，沒有把 F2 底稿誤當持久建築。
- 沒有做 F3／F4／F5、城建循環、荒野生成，也沒有改 M1 管線或加入其他快變數。

## 快／慢隔離：兩組雜湊

固定慢變數與 seed，只改人口（250→2000）或建設等級（1→10）：

```text
site_fill_skeleton_hashes
baseline=8177033870425268075
population=8177033870425268075
development=8177033870425268075

site_fill_output_hashes
baseline=4011855697949024980
population=10271365636982910053
development=6909320556102807185
```

三個 S 雜湊逐位元相同；人口與建設等級各自都改變 F 雜湊。另由 Region 的真歸約 storage 寫入
人口 1000／建設 10 後重切 `SiteFastVars`，骨架雜湊也維持 `5215317409424700076`。

## 同一條程式路徑：村莊 vs 大都會

兩者都走同一個 `populate → assign_site_zones → fill_site_buildings`，`SettlementTier` 只判斷有無
聚落，Village／Town／City 沒有各自的生成分支；差別來自人口／建設等級算出的配額與密度。

```text
             住宅街廓  商業街廓  住宅建築  商業建築
村莊 250/1          1         1        14         3
大都會 8000/20     22         8       253        80
```

packing 測試逐格驗證所有 333 棟大都會建築：不重疊、全在可建地、全在所屬分區且 frontage
整段貼道路；建築占地小於已分區可建面積，確實留下內院／空地。

## 效能（S1～S4 + F1～F2 全跑）

fixture 四邊全為道路，真的跑 A* ×4；人口 100000、建設 100，F1/F2 取高配額與最高密度。
暖機後單次計時，Release 連跑六次：

```text
Debug   3.106 ms
Release 0.431～0.439 ms（worst 0.439 ms）
預算    30 ms
```

兩種組態皆遠低於預算，建置零警告。

## M2.3 三次冷往返重跑

fixture 的 Region 快變數與非預設 `Idle` 持久大廳歸約結果一致（人口 75、建設 1），因此每一輪
都會重建非空住宅／商業分區與程序建築，不是空層假通過。

```text
site_roundtrip_hashes
18117570725830432076 18117570725830432076
18117570725830432076 18117570725830432076
18117570725830432076 18117570725830432076
building_state=Idle(1)
cold_assertions=3 procedural_disk_empty=1 procedural_fill_nonempty=1
procedural_recomputed_after_cache_corruption=1
rematerialize Debug max=0.909388 ms
collapse Debug max=0.068087 ms
```

六點全同；每輪起點皆為未載入的 `L_ABSENT`，程序層不落盤，第三輪能在記憶體程序層被刻意
清空後，從磁碟持久層與 Region 快變數完整重算回相同非空 F1/F2。

## 完整驗證

```text
cmake --build build --parallel 2                    PASS（零警告）
ctest --test-dir build --output-on-failure          167/167 PASS
./build/aetheria_sim --tick 62208000                exit 0
godot-mono --headless --path godot --editor ...     exit 0
godot-mono --headless --path godot --quit-after 5   exit 0
CoreIsolation.CompileCommands                       PASS
cmake --build build-release --target aetheria_tests --parallel 2  PASS（零警告）
```
