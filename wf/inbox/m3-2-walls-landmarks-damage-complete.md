# 信：M3.2 完成 — F3 城牆 + F4 地標 + F5 損毀

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**任務來源**：使用者直接交辦 M3.2

## 完成內容

- Region 只新增本輪真的使用的 `defense`（`uint16_t`）與 `damage`（0～100）兩個快變數；
  `split_site_vars` 投影到 `SiteFastVars`，存檔 v10 → v11，版本沿革表已補。codec 往返及
  normalized world hash 都包含兩欄，負向測試證明任一欄改變都會改世界雜湊。
- F3 的最終邊層仍是 `vector<EdgeId>`，從慢變數骨架 edge 複製後只改填充層；牆、可開關門、
  塔樓、護城河都是 `data/edges.toml` 的 EdgeDef，內外兩格寫入同一 def，沒有半格型別。
  防禦 0 不產牆；高防禦沿較小核心再跑一次形成內外兩重牆。程式尾端另做結果驗證，偵測到
  wall flag 卻沒有 gate+openable flags 會直接拒絕。
- F4 的地標建築與 `owner → landmarks[]` 勢力風格表都在 `data/site_city.toml`；按 owner 從表中
  deterministic 選 def，放到城心距離分數最小、且可放下 footprint 的最高分商業街廓。
- F5 先依損毀度在非城門牆段開缺口，再以「建築中心到最近城門／缺口的 Manhattan 距離」
  canonical 排序，前 N% 改為 Rubble／Burned。缺口與損毀狀態都進 F hash，不進持久層。
- 沒有做城建循環、荒野生成、其他 Region 欄位、M1 管線變更或 M2 生命週期改造。

## 問題 1：零主幹道時的選擇

選擇 **不生成城牆**。M3.0 已裁定沒有 Region 道路 crossing 就不補假門；若 F3 自行補門，
那個門沒有上層道路可以對齊，會破壞跨層一致性。不產牆同時保留該裁定與「有牆必有門」。

明確的零主幹道測試結果：

```text
site_zero_trunk_roads=0 wall_rings=0 wall_edges=0 wall_gates=0
```

另以 16 種四邊道路 mask × 16 個 seed（256 組）重跑：mask=0 一律無牆；其餘每條道路在每重
牆各得到一個可開關門。四主幹道高防禦的實例為 2 重牆、8 個門；防禦 0 為 0 重牆、0 門。

## 問題 2：損毀距離是否真的偏小

是，而且不是邊緣差異。固定四主幹道、防禦 60、損毀 25 的大城：

```text
損毀建築：82 / 326
城門：4，牆缺口：8
損毀建築到最近城門／缺口平均距離：9.08537
全部建築的同一平均距離：20.2515
```

損毀組只有全體平均的 44.9%；測試門檻要求低於 80%，所以偏好有實際生效，樣本也不是因
城門太少而無意義（4 門 + 8 缺口、82 棟損毀）。

## 快／慢隔離

固定慢變數與 seed，分別改人口、建設、owner、防禦、損毀；七組骨架雜湊完全相同，F 雜湊
依快變數改變：

```text
S baseline/population/development/owner/defense/damage
  8177033870425268075（全部相同）
F baseline=3886469848340177665 population=15589177980359671776
  development=17361770242630744896 owner=10380732630900286109
  defense=3184353413305296428 damage=4772004628651865133
```

Region 真 storage 的 defense=100、damage=75 也保持骨架雜湊 `5215317409424700076`。

## 效能（S1～S4 + F1～F5 全跑）

fixture 四邊道路、人口 100000、建設 100，且本輪快變數走實際 F3～F5 路徑；暖機後計時：

```text
Debug   3.68923 ms
Release 0.506666～0.517677 ms（六次，worst 0.517677 ms）
預算    30 ms
```

兩種組態皆遠低於預算，建置零警告。

## M2.3 三次冷往返重跑

```text
expand 1   18117570725830432076
collapse 1 18117570725830432076
expand 2   18117570725830432076
collapse 2 18117570725830432076
expand 3   18117570725830432076
collapse 3 18117570725830432076
building_state=Idle(1)
cold_assertions=3 procedural_disk_empty=1 procedural_fill_nonempty=1
procedural_recomputed_after_cache_corruption=1
rematerialize Debug max=1.56546 ms; collapse Debug max=0.071524 ms
```

六點全同；每輪從未載入的 `L_ABSENT` 開始，程序層不落盤，第三輪能在前一輪故意清空程序
快取後重算。v11 Region 欄位另由 codec 測試往返 `defense=87, damage=42`。

## 完整驗證

```text
cmake --build build --parallel 2                    PASS（四 target，零警告）
ctest --test-dir build --output-on-failure          176/176 PASS
./build/aetheria_sim --tick 62208000                exit 0
godot-mono --headless --path godot --editor ...     exit 0
godot-mono --headless --path godot --quit-after 5   exit 0
CoreIsolation.CompileCommands                       PASS
cmake --build build-release --target aetheria_tests --parallel 2  PASS（零警告）
```
