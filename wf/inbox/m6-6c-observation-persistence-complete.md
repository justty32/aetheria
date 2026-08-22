# M6.6c 新持久欄位進版號與世界雜湊完成回報

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**原任務**：[`done/m6-6c-observation-persistence.md`](done/m6-6c-observation-persistence.md)

## 結果

Site payload 已升至 v17。解碼器讀到舊版 Site tag 時會在解碼持久欄位前明確拒絕，沒有遷移或
補預設值。正規化 Site hash 現在涵蓋 `SiteOrderState` 的存在性與四因子、地點 key、依 uid
排序的具名 NPC 與地城完整持久內容；因此 `missing`、`cleared`、`depth` 均會一路改變磁碟級
world hash。未改 `design/`、`core/ai/` 或三個 Region hash 基準，未 push。

## 驗收實測

| 判準 | 證據 |
|---|---|
| 版號 v17 | `kSaveFormatVersion == 17`；實際舊 v16「只有 buildings」Site payload 被拒。 |
| 拒讀 ≠ 預設 | v16 錯誤：`zone Site payload format_version 不符：檔內=16 預期=17`；另存的 v17 預設 Site 可正常解碼，`order_present=0 named_npcs=0 dungeons=0`。 |
| 四個治安因子進雜湊 | 下表逐一只加 1，七次 world hash 都由磁碟重新解碼計算。 |
| NPC／地城旗標進雜湊 | `missing`、`cleared` 各自翻轉，`depth 3→4`，下表各自改變。 |
| 冷往返 | 非預設 `order={45,25,40,10}`、`missing=1`、`cleared=1`、`depth=4` 寫檔；來源釋放後 `FileZoneStore::load()` 解碼，normalized hash `13039391939482921087 → 13039391939482921087`。沒有比較 bytes。 |
| 決定性 | 同一份磁碟輸入連算兩次 world hash 均為 `2640689774924481653`。 |

### 七個 scalar 各自的 world hash

共同前值為 `2640689774924481653`：

| 單獨改動 | 後值 |
|---|---:|
| `garrison_coverage 45→46` | `8376039522857254017` |
| `patrol_coverage 25→26` | `14546714604980846595` |
| `bandit_pressure 40→41` | `9615805877224320604` |
| `refugee_pressure 10→11` | `670739523845722025` |
| `missing false→true` | `12842999936031071665` |
| `cleared false→true` | `9217672795312099351` |
| `depth 3→4` | `2726570319374566354` |

上述改動值各只選一次，沒有試其他值或為結果重調。

## 六次負向控制（均已還原）

每次從 `normalized_state_hash.cpp` 拿掉指定涵蓋點、固定以
`cmake --build build --parallel 2` 重建，再單跑
`SiteObservationPersistence.EveryObservationFieldChangesWorldHashIndependently`。六次皆 exit 1、
0/1 測試通過；失敗都是 `Expected baseline != changed`，而實際前後相等：

| 注入 | 紅燈欄位 | 實際前值 → 後值 |
|---:|---|---:|
| 1 | 拿掉 `garrison_coverage` | `4065227954165197139 → 4065227954165197139` |
| 2 | 拿掉 `patrol_coverage` | `6441503743189810310 → 6441503743189810310` |
| 3 | 拿掉 `bandit_pressure` | `9155033805736979296 → 9155033805736979296` |
| 4 | 拿掉 `refugee_pressure` | `9781075340808508803 → 9781075340808508803` |
| 5 | 拿掉 `missing` | `14649579784628863770 → 14649579784628863770` |
| 6 | 拿掉地城觀測組 | `cleared` 與 `depth` 都是 `12792131231895804545 → 12792131231895804545` |

任務書同時列出七個 scalar（四因子 + `missing` + `cleared` + `depth`），但硬性指定「六次注入」。
為遵守六次上限，第六次只拿掉同一 `PersistentDungeon` 的兩個觀測 scalar；兩條逐欄斷言各自紅，
所以七個 scalar 都有可觀測漏算證據。沒有私下補跑第七次，也沒有把兩欄一起改當正向證據。

## 三個 Region tile hash 基準的判斷

判斷：三個 tile 值是**必然隨資料 schema 改變的當下 golden snapshot**，不是本來就該跨 schema
恆定的領域不變量。`hash_tiles()` 以 `RegionReductionRows` 展開每列，且每列先 hash 長度再 hash
內容；第五列加入後，即使整列初值為 0，三個值也必然全部改變。因此 M6.6b 的數值變更原因成立，
但這三個常數不適合繼續作為「identity redistribution 行為不變」的主斷言；應改為 identity 前後
的關係斷言（既有 seam 測試的型態），而不是下一次 schema 成長時再更新 snapshot。依任務要求
本輪只給判斷，未修改它們。

## 驗證

- `cmake --build build --parallel 2`：完整 1,318 steps 與後續增量建置成功。
- 聚焦 `SiteObservationPersistence.*`：3/3 綠。
- 全套 CTest：最終重跑 336/336 綠，81.92 秒。
- `./build/aetheria_sim --tick 62208000`：exit 0，抵達第 3 年第 1 季第 1 月上旬。
- Godot 4.7.1 headless editor 首跑、再跑與主場景：三者皆 exit 0。
- `git diff --check`：通過。

## 現有測試證不了的事

- 不證明舊檔遷移；政策與實作都是拒讀。
- fixture 有真實持久型別與磁碟冷載，但不證明 worldgen 已會自動供應具名 NPC／地城。
- 七組具體輸入的 hash 不相等不能數學上排除所有 FNV-1a 碰撞。
- `place_name_key`、NPC／地城 uid 與其他字串／可見性也已納入 hash，但本任務沒有逐欄負向注入；
  本輪只宣稱任務指定的觀測 scalar 偵測力。
- 沒有跨硬體／端序實跑；本輪證據來自目前 x86-64 Linux 環境。
