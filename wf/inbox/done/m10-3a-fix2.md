# 修正輪 M10.3a-fix2 — 崩潰恢復有一個會重複套用的窗口；驗證腳本的負向控制沒有斷言

**寄件人**：波 2 管家｜**收件人**：gpt-sol 實作者｜**工作分支**：`m10-3a`（接著 `ec2a01c` 疊加）
**前輪**：`wf/inbox/m10-3a-fix1-complete.md`（雜湊已合規，431 綠）

## 結論

fix1 的雜湊改法**我驗過了，通過**：我用 Python 獨立重算定序 FNV fold，兩組真實數字都精確命中
binary 的 `world_hash`；交換順序得不同值；v23 基準值我用 `main` 的程式碼獨立算出也相等。
**這部分不要再動。**

但我親手做負向控制時抓到**兩個新問題**，這一輪只修這兩個。

## 問題 1（本輪重點）：崩潰恢復有一個會重複套用的窗口

`turn_commit.h` 寫的順序是「套用 → 存 zone/manifest → **才**前推 marker」。
所以有這個窗口：

```
1. 日誌 append（head=7）
2. marker 落盤，仍持舊值（committed=4）
3. 套用條目 5–7
4. 存 zone/manifest        ← 磁碟上已經是「已套用」的世界
5. 前推 marker → 7
        ↑ 崩在 4 與 5 之間：磁碟＝已套用的 zone ＋ 落後的 marker
```

下次載入時 `playable_session.cpp:269` 的 `recovery_needed()`（`head_seq > committed_seq`）為真，
`replay_tail` 把 5–7 **再套用一次**——世界直接開不起來。

**我親手做的負向控制**（不是重跑你的測試，是我自己改 marker、自己載入）：

```text
--- marker=7（誠實值）:   handcheck LOAD_OK ptr=1        [  PASSED  ]
--- marker=4（模擬崩潰）: C++ exception with description "示範住宅已經蓋過"   1 FAILED TEST
--- marker=7（還原）:     handcheck LOAD_OK ptr=1        [  PASSED  ]
```

⚠ **你的驗收 3 測不到這個窗口**：`set_interrupt_after_journal_for_testing` 的鉤子停在
**步驟 2 與 3 之間**（套用之前），那是**安全的**那一半窗口——磁碟上的 zone 還沒被套用，
所以重放尾巴剛好正確。**危險的是步驟 4 與 5 之間，那裡沒有測試。**

原任務書的 Done-when 寫「套用中途 kill 進程→重啟自動恢復到**一致狀態**」。
現在這個窗口不是恢復到一致狀態，是硬拋。

**要做的**：讓**任何**崩潰點的下次載入都能到達一致狀態（要嘛是已套用的狀態、要嘛是套用前的狀態，
**都不可以是重複套用或硬拋**）。**改法自選，寫進回報**，但有兩個限制：

- ⚠ **不准把 seq 塞進 `SaveManifest`**（原任務書「不要做的事」那條仍然有效，會逼出 bump）。
- 一個可行方向供參考（**不強制**）：偵測到 `recovery_needed()` 時，**不要**把尾巴重放到
  已載入的現役狀態上，改成從「基底 raws＋完整日誌」整個重建——`sim replay` 已經在做這件事
  而且你已經證明它逐位元一致。代價只有崩潰後那一次載入變慢。
  你若有更好的做法，用你的，但要在回報裡說明它為什麼涵蓋**全部**崩潰點。

## 問題 2（繼承缺陷，不是你弄壞的）：驗證腳本擺好了負向控制卻不斷言

`cmake/check_sim_world_hash.cmake` 做了一整套控制：複製 data → 把 `terrain.swamp` 的
`move_cost` 從 3 改成 9 → 用改過的 data 生新槽 → 對新舊兩槽各跑一次 `verify world-hash`。
然後 **從頭到尾沒有比較過那兩筆輸出**——只用 regex 斷言它們「長得像數字」，最後 `message(STATUS)` 印出來。
**把 terrain 改回去，這支腳本一樣綠。**

我查過 `git show main:cmake/check_sim_world_hash.cmake`，**這是 v23 就有的，你這輪只改了 regex**，
不算你的帳。但它就躺在本波主題（世界身分雜湊的偵測力）正中央，一起收掉。

**我親手跑過這個控制，結果值得你知道**：

```text
=== slot-a (baseline terrain) ===        === slot-b (move_cost 3->9) ===
zone_hash=4698212455085739946            zone_hash=4698212455085739946      ← 一模一樣
raws_hash=17134265233666862005           raws_hash=10776467794636923675
world_hash=12771489023462499069          world_hash=11501016914046886720
```

**改地形的 move_cost，`zone_hash` 完全不動**——只有 raws 分量抓得到。
這正是計畫第 6 點要 raws 進雜湊的理由的活證據，所以這條斷言特別值得寫死。

**要做的**：讓該腳本**實際比較**新舊兩槽，並斷言：`raws_hash` 必須不同、`world_hash` 必須不同、
`zone_hash` 在這個特定擾動下**相同**（把「zone 分量對此擾動無感」這件事釘住——
哪天它變了，代表世界生成對 move_cost 的依賴變了，那是該有人知道的事）。
斷言失敗時的訊息要印出兩邊的四個分量。

## 驗收（就這 2 條）

| # | 標準 |
|---|---|
| 1 | **負向控制**：模擬「zone/manifest 已存、marker 未前推」的崩潰點 → 下次載入**不再重複套用**，到達一致狀態；並附**修前紅、修後綠**的輸出。另外**保留**原驗收 3（套用前中止）仍綠——兩個窗口都要有測試 |
| 2 | **負向控制**：`check_sim_world_hash.cmake` 加上比較後，**把 `move_cost = 9` 那段改回 3 讓兩槽相同 → 該檢查必須紅**；改回去 → 綠。附兩次輸出（這是在驗「這支腳本現在真的會因為控制失效而紅」） |

全套 `ctest --parallel 2` 綠，總數不低於 431（新增測試說明分解）。
`kSaveFormatVersion` 仍為 23，不開 enum，不 push，不動 `design/`。

## 不要做的事

- **不要動 fix1 的雜湊組成**（`compose_world_identity`、genesis=0、三分量定序 fold）——我已驗過。
- **不要**把 seq／marker 塞進 `SaveManifest`。
- 不要碰 RulesetPatcher／`add_faction`／`inject/`／SQLite／截斷重放。

## 回報

`wf/inbox/m10-3a-fix2-complete.md`（≤8KB 繁中，首行 `DONE`/`BLOCKED`）：2 條驗收逐項證據
（修前紅／修後綠必附）、恢復改法的最終形狀**與它為什麼涵蓋全部崩潰點**、
格式變更清單增修、以及前兩輪「無法重建清單」照抄保留。

**最重要**：如果你在實作恢復時發現**還有別的崩潰點**也會到達不一致狀態（我只找到一個），
**如實列出來**，不要只修我指出的那一個。那份清單比修好本身更有價值。
