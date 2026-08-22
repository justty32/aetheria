# M6-INT-2 完成回報 — 整合勢力 AI 與 Site 觀測存檔為 v18

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

**任務書**：[m6-int-2-merge-ai-v18.md](m6-int-2-merge-ai-v18.md)

## 結果

已執行 `git merge --no-ff m6-int1-wt`，現行 zone／manifest 格式統一為 **v18**，
同時包含 M6-INT-1 的 `NamedFateLedger`、勢力 AI 三類，以及 M6.6c 的 Site
治安／NPC／地城持久欄位。沒有修改 `design/`，沒有 push。

`decode_zone` 預設 `CurrentOnly`，因此 `FileZoneStore` 每次 `load()` 都只接受 v18；
v14/v15 只能由低階測試明示指定 `LegacyFixture`。v16/v17 不在可解碼清單，
registry 與外交新區塊也都只在 `version == kSaveFormatVersion` 時解碼。

## 合併衝突

1. `cmake/targets_tests.cmake`：同一檔尾追加區塊同時保留
   `site_observation_persistence_test.cpp` 與 `faction_ai_test.cpp`，共用一個完整
   `target_sources(...)`。
2. `tests/zone/zone_codec_test.cpp`：兩邊的舊版輸入收旂為「修改現行檔頭為
   v17，預期 v18 公開載入拒絕」，保留完整錯誤訊息斷言。

## 版本與舊檔拒讀

- `kSaveFormatVersion == 18`：`core/serialize/zone_codec.h:13`。
- 現行／fixture 門檻：`core/serialize/zone_decode.cpp:74-78`。
- registry 只在現行版解新清單：`core/serialize/zone_decode.cpp:177-180`。
- 外交三類只在現行版解碼：`core/serialize/zone_diplomacy_codec.cpp:232`。
- grep `version >= 17|version == 17|version != 17|kSaveFormatVersion = 17` 在
  `core tests cmake sim` 沒有解碼分支，唯一命中是 v17 應被拒絕的測試失敗文字。

實測公開入口錯誤：

- v17 zone：`zone 載入失敗：<path>：zone format_version 不符：檔內=17 預期=18`。
- v15 manifest：`manifest format_version 不符：檔內=15 預期=18`。
- 額外驗證已開啟 store 後才置換 v15 zone 也拒讀：
  `zone 載入失敗：<path>：zone format_version 不符：檔內=15 預期=18`。

## 七類缺席斷言

每類都是獨立 `EXPECT_*`，沒有以集合式訊息代替：

| 類別 | 檔案：行 |
|---|---|
| `NamedFateLedger` | `tests/zone/diplomacy_save_test.cpp:305` |
| `FactionTruth` | `tests/zone/diplomacy_save_test.cpp:307` |
| `KnowledgeRecord` | `tests/zone/diplomacy_save_test.cpp:308` |
| `FactionMindState` | `tests/zone/diplomacy_save_test.cpp:309` |
| `SiteOrderState` | `tests/site/site_observation_persistence_test.cpp:128` |
| `PersistentNamedNpc` | `tests/site/site_observation_persistence_test.cpp:129` |
| `PersistentDungeon` | `tests/site/site_observation_persistence_test.cpp:130` |

## 負向控制（已還原）

只選兩類、各用一個固定注入，沒有試其他值：v15 fixture 解碼後注入空的
`NamedFateLedger{}`，現行空 Site 解碼後注入 `SiteOrderState{}`。

1. 保留兩條缺席斷言：CTest **exit 8、0/2 通過**。
   `NamedFateLedger.empty()` 實際 `false`，預期 `true`；
   `SiteOrderState.has_value()` 實際 `true`，預期 `false`。
2. 保留同一錯誤注入，只移除這兩條斷言：**2/2 通過**。其餘五類斷言沒有
   代替它們抓到漏失。
3. 注入已移除，兩條正式斷言已還原；還原後同二測試 2/2 綠。

## 七個觀測 scalar 雜湊偵測力

共同前值為 `2640689774924481653`，與 M6.6c 數字完全相同：

| 單獨改動 | 後值 |
|---|---:|
| `garrison_coverage 45→46` | `8376039522857254017` |
| `patrol_coverage 25→26` | `14546714604980846595` |
| `bandit_pressure 40→41` | `9615805877224320604` |
| `refugee_pressure 10→11` | `670739523845722025` |
| `missing false→true` | `12842999936031071665` |
| `cleared false→true` | `9217672795312099351` |
| `depth 3→4` | `2726570319374566354` |

連續重算基準值仍為 `2640689774924481653`；冷往返 normalized hash 為
`13039391939482921087 → 13039391939482921087`。

## 最終驗證

- `git diff --check`：通過。
- `cmake --build build --parallel 2`：首次 1321 steps 與後續增量建置均通過，未超過 2 路。
- `ctest --test-dir build --output-on-failure`：**351/351 通過**，0 失敗，
  real time **82.70 秒**。

## 現有測試證不了的事

- 兩個碰撞 v17 都未穩定存在，所以測試證明檔頭 v17 一律拒讀，不宣稱能從
  payload 內容辨識它來自 M6.6c 或 M6-INT-1。
- 此次數字只證明固定 fixture 的七個 scalar 對 FNV-1a 有可觀測影響，不能數學上
  排除任意輸入的 hash 碰撞。
- 冷往返不證明跨平台逐位元輸出一致，也不是對勢力 AI 演算法與校準值的
  重新推導；本輪只做整合。
