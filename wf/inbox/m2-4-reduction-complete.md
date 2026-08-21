# 信：M2.4 歸約通道完成

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`

## 完成內容

- 實作兩列固定歸約量：`PopulationReduction`、`DevelopmentLevelReduction`。
  M2.4 只有 `SettlementHall`，因此人口暫以建築狀態得到 `100/75/25/0`，
  建設等級以 `Active/Idle=1`、`Derelict/Ruined=0`；沒有實作其餘八列。
- `RegionTiles` 新增對應快變數 storage，存檔格式 **v9 → v10**；兩列均進位元流與
  正規化狀態雜湊，`has_live_site` 是執行期狀態、不進存檔。
- `reduce_live_site_xun` 是每旬入口。Region 旬回合發現任一 live Site 時，若沒有提供
  歸約 pass 會直接 throw；測試證明提供後該旬恰執行一次，再進 Region 第 5 階段。
- `collapse_site_zone` 在 `ZoneManager::unload` 前強制呼叫同一歸約入口；歸約或驗證失敗
  就不卸載。成功後才清掉 `has_live_site` 並回到 `L_ABSENT`。

## 1. 結構性摩擦：怎麼擋繞過表直接寫 Region

唯一 schema 是 `RegionReductionRows` type list。storage 與 `RegionTileDelta` 都由它做
template transform 自動展開；新增一列只要新增 row type、把它加到清單尾端並提供該列的
量測／近似公式，不必改 delta 或 apply 機制。

Region 的 reduction storage 是 `RegionTiles` **private** 成員，公開介面只有唯讀 span／value，
沒有 setter。Site 端唯一 friend 是 `ReductionTable`；`RegionTileDelta` 的建構子也是 private，
呼叫端不能自行拼出 delta。另兩個合法 writer 是 Region 自己的 `RegionSimulation`，以及只負責
v10 位元流的 codec friend。編譯期負向檢查證明 `RegionTiles` 沒有 public `population` 欄位，
且 `RegionTileDelta` 不能 default construct。測試另改動未列入表的 procedural zoning，
歸約值保持不變。

所以一般 Site 程式碼無法寫目標欄位；要讓新量影響 Region，必須顯式新增 row，這就是摩擦。

## 2. has_live_site 負向控制與時機

負向控制使用 Region 公式的執行／跳過報告，並刻意讓兩條公式給不同人口：

```text
has_live_site=true:  formula_execution_count=0, live_site_skip_count=1, population=75
has_live_site=false: formula_execution_count=1, live_site_skip_count=0, population=100
```

這不是只比最終結果；計數器直接證明 live 時 Region 公式沒有執行。旬回合另有缺 pass 的
throw 負向測試，避免 live Site 靜默沿用上旬數字。

卸載強制歸約測試把 Hall 改成 `Derelict` 後直接 collapse：

```text
collapse_reduction population=25 development=0 has_live_site=0 format_v=10
```

解碼 v10 Region 後仍為 `population=25, development=0`，而 runtime 的
`has_live_site` 正確回到 false。

## 3. 校準誤差

固定 seed 隨機取樣 1000 個合法 Region tile 狀態；聚落格限制為可建陸地，非聚落格仍會取樣
不同 terrain，另隨機 relief、feature、elevation、owner、settlement。沒有為測試調參數。

```text
population:        min=0, median=0, p95=0, max=0, >=5%=0
development_level: min=0, median=0, p95=0, max=0, >=5%=0
```

這輪是最小 `SettlementHall` surrogate：`project` 對任何非 None 聚落建立一棟 Active Hall，
Region 近似公式也對同一狀態給 `100/1`，所以目前恰好完全吻合。M3 加住宅等真實 Site 算法後，
同一測試會開始量出實際近似誤差，沒有把未來的十列表先鋪進來。

完整歸約入口（reduce + apply）Debug 實測 **0.000191 ms**，低於 30 ms。

## 4. 加歸約後的 M2.3 冷往返

三次冷往返、每輪展開／收回各一點：

```text
18068079511531492422 × 6（全同）
building_state=Idle(1)
cold_assertions=3
procedural_disk_empty=1
procedural_recomputed_after_cache_corruption=1
rematerialize Debug max=0.511314 ms
collapse Debug max=0.068248 ms
```

負向控制仍在原斷點斷開：

```text
Idle → Derelict
collapsed=18068079511531492422
mutated=13476801997497996951
expanded=13476801997497996951
```

答案：**仍穩定**。歸約是整數、固定 row 順序、無 unordered traversal；寫回 Region 也沒有
進 Site 骨架或持久層，因此本輪沒有不決定論，也沒有形成回饋迴圈。

## 完整驗證

```text
cmake --build build --parallel 2                    PASS（四 target，零警告）
ctest --test-dir build --output-on-failure          153/153 PASS
./build/aetheria_sim --tick 62208000                PASS
godot-mono --headless --path godot --editor ...     exit 0
godot-mono --headless --path godot --quit-after 5   exit 0
CoreIsolation.CompileCommands                       PASS（core 零 godot-cpp）
```

未做事件升級、其餘八列、建築／城建／M3 內容或 M1 生成管線修改。
