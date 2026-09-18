# M7-INT-5 Local 戰鬥整合與真 Site 校準完成回報

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**狀態**：完成

## 合併與衝突

已執行 `git merge --no-ff m7-3-wt`。兩個衝突都是同一種括號陷阱，逐段解決：

- `cmake/targets_core.cmake`：先完成 Site 的 `target_sources(aetheria_core ...)`，再開獨立的
  Local `target_sources`，兩者各有自己的右括號。
- `cmake/targets_tests.cmake`：同樣先完成 Site 測試 source 區塊，再加入完整 Local 測試 source、
  負向 executable 與 CTest 區塊。

`git diff --check` 通過。

## 校準對象與薄介面

N=1000 測試不再透過舊 M6.7 名義的
`resolve_scaled_combat(..., CombatLayer::Site, ...)` 取樣，而是明確依正確順序執行：

1. `resolve_region_combat` 產生 Region 基準；
2. 真 `site::simulate_site_battle` 以 Region 傷亡配額產生 Site 結果；
3. `SiteBattleResult::loss_a/loss_b` 經 `LocalCombatRoundInput::site_expected_loss` 傳給 Local。

真 Site 的輸出已能一對一接上既有兩欄陣列，所以薄介面不必改型別或新增耦合；只更新過時註解，
明定輸入來自真 Site。沒有修改 Region、Site 或 Local 參數，也沒有為數字調參。

## N=1000 真 Site 校準

固定 `R=0.7500..1.2490`，三個 bins `<0.9 / 0.9..1.1 / >1.1` 的樣本數為
`300 / 402 / 298`。

- A 平均損失 Site/Local：`14333.6 / 14332.3`；有號相對誤差 `-0.00872775%`。
- B 平均損失 Site/Local：`14278.3 / 14277.1`；有號相對誤差 `-0.00871952%`。
- A+B 有號相對誤差：`-0.00872364%`。
- 六個 bin 的 Local−Site 有號相對誤差：
  - low A/B：`-0.0146548% / -0.0185374%`
  - mid A/B：`-0.0108761% / -0.0108826%`
  - high A/B：`+0.000404821% / +0.000412988%`
- 符號：A 為 `−/−/+`，B 為 `−/−/+`；兩側各自同時含負、正號。
- 方差 Site/Local：`5.18186e6 / 1.13483e7`，Local 明確較大。

相較 M7.3 對舊校準面的數字，均值、誤差與方差均如實改由真 Site 決定；本固定樣本的最大 bin
絕對誤差是 `0.0185374%`。未修改任何參數以美化結果。

## +3% 同向偏差負向控制（真的紅）

暫時在 Local 微觀變異完成後，將 A、B 傷亡都乘 `1.03`（仍受 side power 上限約束），完整
增量建置後單跑
`LocalCombat.ThousandBalancedSamplesTrackSiteWithMixedSignsAndHigherVariance`：exit `1`。

紅的是混合符號斷言：實際 `has_negative={false,false}`，預期 `{true,true}`。六個 bin 全部轉正：

- low A/B：`+2.98488% / +2.98097%`
- mid A/B：`+2.98878% / +2.98884%`
- high A/B：`+3.00063% / +3.00052%`

整體 A/B/合計誤差為 `+2.99106% / +2.99109% / +2.99107%`，故障時方差 Site/Local 為
`5.18186e6 / 1.20394e7`。數字取得後已還原注入、重建，原測試恢復綠燈。

## 驗證與範圍

- `cmake --build build --parallel 2`：通過（含 `-Werror`）。
- `ctest --test-dir build --output-on-failure`：`402/402` 通過，`85.47 s`。
- Region、真 Site、Local 三層戰鬥測試均在上述全套中通過。
- `kSaveFormatVersion` 維持 `20`；未新增持久欄位。
- 未修改 `design/`、`bridge/`、`godot/`，未 push。
