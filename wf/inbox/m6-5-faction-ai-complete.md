# M6.5 完成回報 — 勢力 AI

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者

## 結論

已落地六目標慣性、七項資料性格、整數效用、`faction_id` 同分裁決、三級 AI LOD、
情報誤差、均勢、連鎖參戰與玩家代管。受限 AI target 仍只看 `FactionView`；世界層把戰鬥
評估直接轉交 M6.3 `rules::resolve_region_combat`，再由玩家／AI 共用的
`execute_faction_command` 執行命令。Region 七階段的 `FactionAiPass` 每旬可呼叫
`advance_all_factions_ai_xun`，後者依 faction_id 逐一執行全部勢力。

zone 格式升至 v16：`FactionTruth`、`KnowledgeRecord`、`FactionMindState` 都進持久層與
正規化雜湊；v15 載入時三欄是明確「缺席」。

## 驗收實測

| 判準 | 實測 |
|---|---|
| 目標慣性 | 固定局勢跑 100 旬：門檻 80 切換 **21** 次；負控門檻 0 為 **84** 次 |
| 性格真的接上 | 同局勢／seed：好戰型選 **DeclareWar**，重商型選 **Develop** |
| 決定性 | 同效用且輸入順序正反兩組，皆選 target faction_id **2** |
| 與玩家同公式 | `forecast_region_battle` 直接呼叫 `resolve_region_combat`；2000 對 800，AI 預測勝、玩家公式 `SideBRouted` |
| 玩家不是特例 | 玩家代管與 NPC 都呼叫 `advance_faction_ai_xun`；命令與執行後真值逐欄相同 |
| 全勢力旬入口 | 三勢力一次各跑 1 次，模式 Full／Simplified／Statistical；Region pass 五旬收到 5 個正確 tick |
| observer 場強 | Site streaming 與 AI 都呼叫 `observer::field_score`；遠距 marked 仍由子 observer 強度拉到 Full |
| 三級觸發數 | 24 samples × 1000 旬：**24000／24000／24000**，無 0 級 |
| 三級實際成本 | 每旬效用／目標評分次數：完整 **7**、簡化 **3**、統計 **0**；簡化未呼叫完整 AI |
| LOD 無偏 | 簡化相對完整 **0 permyriad**；統計相對完整 **0 permyriad** |
| 偏差符號 | 簡化個別樣本 **+16／−8**；統計 **+8／−16**，不是固定同向 |
| 誤判戰敗 | 1000 旬中，因低估且預測可勝而宣戰、用真值公式承受較大損失：**61 次** |
| 情報品質 | 遠距／低關係 uncertainty **8000**；近距／高關係 **100** |
| 均勢 | 目標國力 1400→5600，敵對效用 **594→1194** |
| 連鎖參戰 | 防禦盟友履約產生對攻擊者戰爭；拒絕時被保護方對盟友 trust **−5000** |
| 效能 | 15 次完整決策；暖機後固定 **N=5** 取最小：**0.037901 ms** |
| 存檔 v16 | 冷載入 normalized hash **10090946477089852827 → 同值**；真值 3579、情報 2 筆、目標切換 9 與決策皆保留 |
| 舊版語意 | v15：truth／knowledge／mind presence 全為 **0**；不是全知或全不知值 |

## 無偏校準過程（未隱藏嘗試）

沒有反覆試玩法參數；`goal_switch_threshold=80` 只選一次，0 只作指定負控。

LOD 第一次量測誤把三組初始總國力設成 2100／2000／2200，得到簡化 −5.41%、統計
+5.40%，判定是 fixture 錯誤後改成相同起點。相同起點下，獨立 hash 抖動得到 −0.49%／
+0.28%，大小過關但每級符號仍固定，故不接受。最後改成三旬 `−1/0/+1` 配額守恆循環，
seed 只旋轉相位；不是再試數值。最終平均誤差 0，且個別樣本同時有正負號。

## 真正紅燈與負向控制

1. 暫時移除 faction_id tie-break、只保留容器第一筆：
   `FactionAiDeterminism.EqualUtilityUsesFactionIdNotIterationOrder` 紅，forward 實際 target
   **3**、預期 **2**。確認後已還原。
2. v16 解碼後暫時 `state.knowledge.reset()`：冷載入測試紅；hash 從
   **10090946477089852827** 變成 **2995833069633233958**，且
   `persisted.knowledge.has_value()` 實際 false、預期 true。確認後已還原。
3. 目標門檻直接以正式測試設為 0：100 旬切換 **21→84**，明顯暴增。
4. 初次編譯曾因把 AI 欄位塞進既有 `FactionRules` aggregate，使四個 worldgen 測試以
   `-Werror=missing-field-initializers` 紅；已改成同層獨立 `FactionAiRules`，舊三欄 API 不變。

## 驗證與限制

- `cmake --build build --parallel 2`：通過，全程未高於 2 路。
- `ctest --test-dir build --output-on-failure`：**333/333** 通過（提交前最終完整重跑）。
- `aetheria_sim --tick 62208000`：exit 0。
- Godot headless editor／主場景：皆 exit 0。
- 未動 `design/`、`core/fate*`、`core/narrative/`、`godot/`、`bridge/`，未 push。

現有測試證不了：跨 ABI 的逐位元一致；真實長局的外交品質；實際軍隊座標尚未進
`FactionCommand`，目前只把觀測取得的 coarse route cost 持久快取並重用於評分，沒有可供
本輪量測的 12288 格 A* 行軍 caller；玩家是否接受同盟提案也尚無談判流程。這些沒有以假
fixture 宣稱完成。
