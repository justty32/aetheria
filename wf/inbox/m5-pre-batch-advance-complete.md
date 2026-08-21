# 信：M5-pre 批次推進完成

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**任務來源**：[`m5-pre-batch-advance.md`](m5-pre-batch-advance.md)

## 完成內容

- `SiteTurnPipeline` 新增真正的批次入口；呼叫者一次交入同一 Region 的一組 `L_FULL` Site。
- 批次先依 `ZoneKey` 排序。每個小時依該順序推進全部 Site；共同旬界時在同一個
  `RegionTurnPipeline` 歸約 callback 內依序收齊全部 Site，再只結算 Region 一次。
- 原單 Site 入口只把自身包成一元素批次並取回該筆報告，不再保留第二套推進演算法。
- 批次拒絕空指標、重複 Site、非 `L_FULL`、ZoneKey／Region 落點不符、Region 未標記 live，
  以及旬內小時不同步的輸入；避免永遠無法到達共同旬界。
- 沒有新增原始碼檔、沒有修改 `design/`，也沒有加入串流、場強、載入／卸載或預生成。

## 驗收實測

| 判準 | 實測數字 | 結果 |
|---|---:|---|
| 兩個 Site 各推進 240 小時 | Site=2、Site-hours=480 | 通過 |
| 旬界 Region 結算次數 | Site 旬界=2、Region 旬結算=1 | 通過 |
| 旬界歸約寫回 | 2 次；兩個 live 人口皆由 100 推進為 99，Region 兩格皆為 99 | 通過 |
| 未載入格的 Region 算術 | 批次一次結算後人口=104 | 通過 |
| 輸入順序無關 | 正序／反序的 Region + 兩個 Site 正規化 hash 共 3/3 相同 | 通過 |
| 回報順序正規化 | 反序輸入的兩筆回報仍為 `ZoneKey[0] < ZoneKey[1]` | 通過 |
| 單 Site 等價 | 12/12 `SiteAdvanceReport` 欄位、Region/Site hash 2/2 相同 | 通過 |
| 單 Site 路徑計數 | Site-hours=240、Site 旬界=1、Region 旬結算=1 | 通過 |
| 不在途中降級／逐出 | 批次後 2/2 Region tile LOD 仍為 `L_FULL` | 通過 |

批次報告另帶總歸約次數、旬界歸約次數、Site-hours、Site 旬界與 Region 旬結算計數；上述量測
全部為非零真路徑。這輪沒有新增 wall-clock 效能斷言，因此沒有新增可違反暖機／min-of-5 規約
的量測。

## 負向控制：舊形狀會得到可辨識的雙算值

刻意對同一 Region 的兩個 Site 各呼叫一次單 Site 入口、各推進 240 小時，未載入城鎮的人口
得到 **108**；正確批次值是 **104**。Region 近似公式的實際序列為：

```text
初始 reduction=0 → 第一次 Region 旬結算=104 → 第二次 Region 旬結算=108
```

因此 108 不是籠統的「不相等」，而能直接辨認 Region 被推進兩旬。若批次實作退化成逐 Site
各結一次，測試的 `batch_population == 104` 會拿到 108 而失敗；負向控制具偵測力。

## 存檔位元流

**沒有變。** 本輪只新增執行期借用 target／報告型別與批次控制流程；未改任何 `serialize()`
欄位、`AllComponents`、Region storage 或 zone codec。`kSaveFormatVersion` 維持 **13**，不需升版，
版本沿革表也不需新增紀錄。

## 現有測試證不了的事

1. M5 世界時鐘／串流協調器尚未落地，現有測試只能證明「交入完整批次時」的語意，不能證明
   未來協調器必定沒有漏掉某個 `L_FULL` Site。
2. 現有 Region 模擬對 live tile 會跳過近似公式，沒有跨 Site 的 Region 副作用可當黑箱探針；
   測試能證明兩次歸約都發生且 Region 只結算一次，但「兩次歸約皆在 Region stage 5 前完成」
   仍主要由單一 callback 內先跑完整迴圈的控制流保證。
3. 真正的場強重算、LOD 升降與逐出協調器不在本輪範圍；目前只能證明批次推進本身沒有改變
   兩個 Site 的 LOD，不能做未來整合後的回合尾端順序端到端驗證。

## 驗證

- `cmake --build build --parallel 2`：通過，全 target、零警告。
- 批次與單 Site 等價針對測試：2/2 通過。
- `ctest --test-dir build --output-on-failure`：**204/204** 通過（原 202 項全通過，新增 2 項）。
- `CoreIsolation.CompileCommands`：通過，`aetheria_core` 零 godot-cpp 依賴。
- `./build/aetheria_sim --tick 62208000`：exit 0。
- `git diff --check`：通過。

未 push。
