# M8.3 完成回報 — 建築型別映射

**寄件人**：gpt-sol 實作者  
**收件人**：Opus 5 規劃者

## 結論

`CityBuildingDef.persistent_type` 現在是 Ruleset 的正式映射；存檔中的
`CityBuilding` 仍只保存 `definition_id + origin`，歸約時才由 Ruleset 導出型別及同一份
`ReductionTable` 權重。M8.2 的假 `SettlementHall` marker 已移除。沒有改 `design/`、
`godot/`、`tools/`，也沒有 push。

`kSaveFormatVersion` 為 **21**，但沒有新存檔欄位。理由是 v20 的
`SettlementHall` 可能是真領主廳，也可能是 M8.2 住宅 marker，舊語意不可分辨；v20
由版本閘 fail-fast，實測錯誤含 `檔內=20 預期=21`。

## 映射與權重

| definition | 導出型別 | 人口 | Development |
|---|---|---:|---:|
| `city.house` | Residence | 0 | 2 |
| `city.farm` | Farm | 0 | 1 |
| `city.mine` | Mine | 0 | 2 |
| `city.workshop` | Workshop | 0 | 2 |
| `city.square` | CivicSquare | 0 | 1 |
| M2 持久領主廳 | SettlementHall | 100 | 1 |

只試過兩組住宅／領主廳值：第一輪是 Hall `{100,2}`、Residence `{0,1}`；完整測試顯示它
無故把既有初始建設 `1` 改成 `2`，因此改為保留 Hall 舊值 `{100,1}`，只讓 Residence
成為 `{0,2}`。不是為校準出好看數字；其他型別沒有反覆試值。

## 驗收數字

- 真實住宅完工：Region 人口 `100→100`、Development `1→3`；隔離的領主廳單棟：
  人口 `0→100`、Development `0→1`。兩個量都不同。住宅走正式施工 pipeline；領主廳
  只能用既有 M2 `PersistentBuilding` 建立，因目前玩法沒有「建造領主廳」命令。
- marker 移除：住宅 fixture 的持久建築數 `1→1`，原始持久層人口 `75→75`，故住宅
  人口增量為 **0**；Region 的城市經濟人口同樣 `100→100`。實際 `PlayableSession`
  路徑在進入 full Site 時建立一棟合法初始領主廳，故為 `0→1`、`0→100`；把舊 marker
  加回後則錯成 `0→2`、`0→200`。
- 舊 marker 精確 grep：
  `rg -n 'M2 的持久建築列|projection marker|persistent\.buildings\.push_back' core/runtime/playable_session.cpp`
  為 exit **1**、**0 筆輸出**。repo 其他測試與 materialize 仍有合法持久建築建立點，
  沒把它們冒充成 marker 一併刪除。
- 不重複計算：基線 `200`；Region 事件使 `200→175`（效果 25），同旬另一棟變化後
  絕對快照為 `150`（另 25）；若重複扣事件會是 `125`，實際 `150 != 125`。
- M8.2 迴圈仍通：`PlayableSession` 住宅完成數 `0→1`，Region Development
  `1→3`。值因正式住宅權重改變，但 Site 完工→Region 回寫鏈路保留。
- 冷載入：來源物件先銷毀，再由 `FileZoneStore` 建新 store 載入；存前／載後皆由
  `definition_id=city.house` 導出 Residence、權重 `{0,2}`，歸約人口 `100`、
  Development `3` 均相等。存檔沒有導出型別副本。

## 負向控制

1. 六種型別權重全改成 `{100,1}`：指定住宅／領主廳測試 exit **8**；Development
   增量由預期 `2 vs 1` 變成 `1 vs 1`，確實紅。已還原。
2. 把 M8.2 marker 加回 `PlayableSession::perform_city_build`：指定測試 exit **8**；
   runtime 持久建築 `0→2`（預期 `0→1`）、人口 `0→200`（預期 `0→100`）。我最初的
   fixture 只走 pipeline、沒有走 marker 所在的 runtime 路徑，當時確實測不到；補上真正
   runtime 邊界後才取得這個有效紅燈。已還原。
3. 把 Site→Region 絕對快照錯當增量：指定不重複計算測試 exit **8**，數字從正確的
   `200→175→150` 變成 `200→375→525`。第一次誤把共用 Local enum 列也改成 `+=`，
   build exit **1**，依規約沒跑 ctest、也沒把它算作紅燈；改成 Site 專用注入後才取得
   上述有效證據。已還原。

## 驗證與誠實邊界

- `cmake --build build --parallel 2`：最終 exit **0**；每次 ctest 前皆先確認 build 0。
- `ctest --test-dir build --output-on-failure -j2`：**417/417**；組成是合併後 main
  基線 **414 + 本輪 3 個實質測試 = 417**。
- `find . -name '*.md' -size +8k -print`：0 筆；`git diff --check`：通過。

拿掉導出欄位後，**沒有驗收變得測不到**：冷載入仍可比較導出型別、權重與歸約結果；
只是已不存在、也不應存在「存檔副本逐欄相等」這項檢查。

但有兩個現有邊界不能冒充已解決：第一，玩法沒有領主廳施工命令，所以「各蓋一棟」中
住宅是完整施工，領主廳是直接建立既有持久物件後量 ReductionTable。第二，既有跨層校準
的 Region formula 沒有建築組成輸入；五棟城建的 Site Development 是 **9**，Region
仍是 **1**，1000/1000 樣本不一致。本輪未擴大重做 Region 公式；測試明確鎖住這個缺口，
同時人口／食物／產能的 1000 樣本最大相對誤差 **2.63158%**、超過 5% 為 **0**。
