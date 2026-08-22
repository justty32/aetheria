# M6-INT-1 完成回報 — M6.5 合併與 zone v17

**寄件人**：gpt-sol 實作者

**收件人**：Opus 5 規劃者

**任務書**：[m6-int-1-merge-ai.md](done/m6-int-1-merge-ai.md)

## 結果

已用 `git merge --no-ff m6-5-wt` 合併勢力 AI，現行 zone／manifest 格式統一為
**v17**，同時包含 `NamedFateLedger` 與勢力 AI 三類持久狀態。公開存檔入口只接受 v17；
v15 manifest 與碰撞版本 v16 zone 都大聲拒讀。沒有修改 `design/`，也沒有 push。

低階 `decode_zone` 仍保留 v14/v15 fixture 解碼，僅供既有「缺席不等於預設值」測試；
執行期 `FileZoneStore` 不接受舊 manifest，因此沒有 v15 → v17 遷移路徑。

## 三個衝突

1. `cmake/targets_core.cmake`：保留 M6.4 的 `named_fate.cpp`、
   `ruleset_load_world_observations.cpp` 與 M6.5 的兩個 faction AI source；逐塊補齊各自
   `target_sources(...)` 結尾括號。
2. `cmake/targets_tests.cmake`：保留 named-fate 測試／負向 target 與 faction AI 測試，
   各自關閉 `add_test(...)`、`target_sources(...)`，未共用衝突塊外的右括號。
3. `tests/zone/diplomacy_save_test.cpp`：自動合併額外產生兩份同名 v15 encoder，已合成一份
   真 v15 fixture（`AllComponentsV15` + v15 外交區塊）。兩側測試改為：
   `V15FixtureKeepsAllV17FieldsAbsent` 集中驗四類缺席；
   `V17StoreRejectsV15Manifest` 驗公開入口拒讀。

## 版本與拒讀證據

- `kSaveFormatVersion == 17`：`core/serialize/zone_codec.h:13`。
- registry 與外交新增 payload 都只從 v17 分支讀：
  `core/serialize/zone_decode.cpp:169`、`core/serialize/zone_diplomacy_codec.cpp:230`。
- grep `version >= 16|version == 16|version != 16|kSaveFormatVersion = 16` 在
  `core tests cmake sim` 無解碼／版本常數命中；v16 只作測試故障輸入。
- v15 公開載入錯誤：
  `manifest format_version 不符：檔內=15 預期=17`。
- v16 zone 解碼錯誤：
  `zone format_version 不符：檔內=16 預期=17`。

## 四類缺席斷言

`DiplomacySave.V15FixtureKeepsAllV17FieldsAbsent` 的四個互相獨立斷言：

- `NamedFateLedger`：`tests/zone/diplomacy_save_test.cpp:304`
- `FactionTruth`：`tests/zone/diplomacy_save_test.cpp:306`
- `KnowledgeRecord`：`tests/zone/diplomacy_save_test.cpp:307`
- `FactionMindState`：`tests/zone/diplomacy_save_test.cpp:308`

四者都驗 `empty()`／`has_value() == false`，沒有拿預設內容冒充缺席。

## 負向控制

只做一組、沒有試數值：暫時在 v15 fixture 解碼後注入一個 `NamedFateLedger`。

1. 保留 NamedFate 缺席斷言時，測試確實紅，exit 1、0/1 通過：
   `Actual: false`、`Expected: true`，失敗位置為該 `view.empty()` 斷言。
2. 保留同一個錯誤 fixture，只移除 NamedFate 斷言後，測試變成 exit 0、1/1 通過；
   另外三類斷言沒有代替它抓到 NamedFate 漏失。
3. 已移除注入並恢復正式斷言；最終全套綠。

## 驗證數字

- `git diff --check`：通過。
- `cmake --build build --parallel 2`：通過；首次完整 build 1320 步，後續皆同樣限定 2 路。
- 指定測試：5/5 通過（v16 拒讀 + 四個 DiplomacySave 測試）。
- 最終 `ctest --test-dir build --output-on-failure`：**347/347 通過**，0 失敗，
  real time **82.10 秒**。
- 冷往返 normalized hash：
  **16444834826718088955 → 16444834826718088955**。
- 同次 world hash：**11028896895762153064**。

## 現有測試證不了的事

- 沒有「真實穩定 v16」fixture，因兩份 v16 從未形成單一格式；測試證明 header=16 必定拒讀，
  不宣稱能辨識兩份碰撞 payload 的內容來源。
- 冷往返雜湊證明本 fixture 的持久狀態相同，不等於跨平台逐位元輸出相同，也不證明任意不同
  entity 建構歷史會有相同 bytes。
- 本輪只做整合，沒有重新推導或調整 `core/ai/` 演算法與校準值。
