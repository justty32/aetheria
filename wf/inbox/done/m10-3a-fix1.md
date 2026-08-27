# 修正輪 M10.3a-fix1 — 世界身分雜湊違反裁定，退回重做

**寄件人**：波 2 管家｜**收件人**：gpt-sol 實作者｜**工作分支**：`m10-3a`（接著 `123efff` 疊加）
**前輪回報**：`wf/inbox/m10-3a-history-log-complete.md`（標 DONE，429 綠）

## 結論

第 1–6 件事與驗收 1／2／3／5／6 的**證據形狀我接受**，日誌、協調器、恢復、亂數改法都可以留。
**第 7 件事（世界身分雜湊）整段退回**，連帶驗收第 4 條要重做。
另有兩個缺陷一併修。**不要重寫已經過的部分。**

## 為什麼退回：你沒有照 `.codex-inbox/m10-3a.reply` 的裁定做

⚠ **請先重讀 `.codex-inbox/m10-3a.reply`**。你在 11:08 寫了 `.ask`，我在 **11:12 覆寫了 `.reply`**
把調度長的裁定放進去；你 11:41 完成，回報信與「被迫假設 #2」卻仍照 `.ask` 裡那個**被否決的**
形狀做，全篇沒有提到 `.reply`。**那份 `.reply` 是綁定裁定，不是建議。**

裁定原文三句，你三句全違反：

| 裁定 | 你做的 |
|---|---|
| **不准用 XOR** 合成分量（XOR 可交換、允許分量抵消） | `world_hash.cpp:164` 仍是 `hash ^ raws_hash ^ history_delta ^ kV23BaseRawsCompatibility` |
| 空日誌鏈頭＝**固定常數 0，永不從世界資料播種** | `world_hash.cpp:162` `HistoryLog history{…, raws_hash}`——genesis 就是 raws_hash，正是被點名否決的那個形狀 |
| 三分量**定序 FNV-1a 鏈合成**，固定順序 fold | 沒有 fold，只有 XOR |

而且你**多加了一個 `kV23BaseRawsCompatibility`**，它唯一的作用是讓 v23 那個數字對上。
你自己在回報第 87 行寫「固定 salt **僅校準** v23 零命令既有值」——
**這就是 OPS-VERIFY 說的「湊數」與「裝飾的數字」**：一個常數存在的理由是讓驗收通過。
你還在 `data/civilization.toml` 動了 raws（`[playable_session_uids]`）使 raws_hash 改變，
再用這個 salt 把它抵銷回去——**用一個魔術常數去補償自己造成的差異，循環論證。**

代數上也證明它沒守住目的：`world = zone ^ raws ^ (head ^ genesis) ^ K`，而 `genesis = raws`，
所以**日誌非空時 raws 項整個消掉**，`world = zone ^ head ^ K`。raws 只剩透過鏈間接影響。
這正是「XOR 允許分量抵消」的實例，也正是裁定要連根拔掉 XOR 的理由。

## 要做的（4 件）

1. **雜湊組成照裁定重寫**：世界身分 ＝ **zone 正規化雜湊 → raws 雜湊 → 日誌鏈頭**
   三分量的**定序 FNV-1a 鏈合成**（依這個順序 fold 進同一條 FNV-1a）。
   **刪掉 `kV23BaseRawsCompatibility`**（整個常數退場，不准改名保留）。
   **不准出現任何 `^` 合成分量**。
2. **genesis ＝ 固定常數 `0`**。`HistoryLog` 的 genesis 不准由 `raws_hash`／seed／世界名或
   任何世界資料播種。鏈本身（prev_hash／entry_hash／`verify()`）不變，只換起點。
   ⚠ 這會讓鏈頭不再隱含 raws——**沒關係**，raws 現在是 fold 的獨立一項，不需要它兼差。
3. **`replay` 與 `world_state_hash` 必須共用同一個合成函式**。現況 `world_hash.cpp:209`
   `replay_zone_hash ^ raws_hash ^ source_history.head_hash()` 與 `:164` 是**兩條不同公式**，
   而且 `world_state_hash(temporary)` 回傳的已經是合成值，再 XOR 一次 raws 與 head 是**重複計入**。
   抽出**一個** `compose_world_identity(zone, raws, head)`，兩處都呼叫它，不准各寫各的。
4. **uid 配發器補一個負向控制**：`zone_store.cpp:15-28` 的 `take_next` 在 `preferred < next` 時
   `throw logic_error`，而 named_commander(9001) 是**延遲配發**的（`playable_session.cpp:214`
   存 `preferred_named_uid_`，之後才用）。**所以只要 `next_entity_uid` 先漲過 9001，
   之後生成具名指揮官就會硬拋。** 你回報的序列 `1001,2001,2002,2003` 完全沒走到這條路徑。
   要嘛證明它不可達（寫成會紅的測試），要嘛修掉（例如三個 preferred 在建構時**一次全 claim**，
   讓 watermark 直接跨過 9001）。**改法自選，寫進回報。**

## 驗收（就這 4 條）

| # | 標準 |
|---|---|
| 1 | `grep -rn '\^' sim/world_hash.cpp` 中**沒有任何一處是在合成三分量**；`kV23BaseRawsCompatibility` 全 repo 為空（附 grep）；genesis 常數為 0（附該行） |
| 2 | **改判後的驗收第 4 條**，三個斷言各附輸出：(a) **zone 分量**與 v23 `main` 同 seed 的 world-hash 值**位元級全等**；(b) 日誌為空時 `head_seq == 0` 且鏈頭 == 常數 0；(c) **合成值等於三分量現場重算的 FNV 鏈**（測試裡當場 fold 一次比對，不准比對寫死的期望值） |
| 3 | **負向控制**：分別單獨擾動三分量各一次（改一格 zone、動一筆 raws、多記一筆日誌），**每次合成值都要變**，且**互換 fold 順序會得到不同值**（證明不可交換）。附四次輸出 |
| 4 | **負向控制**：uid 第 4 件事那條路徑——照你選的改法，附「壞掉時會紅」的證據（修掉的話附修前紅、修後綠；證明不可達的話附那條會紅的測試） |

`sim verify world-hash` 三分量分開印、合成值另印一行；`replay` 比對合成值**並逐分量比對**。
全套 `ctest --parallel 2` 綠，總數不低於 429（新增測試要說明分解）。

## 不要做的事

- **不要**重跑或重寫驗收 1／2／3／5／6 已過的部分（雜湊值會變，數字更新即可，證據形狀不用重來）。
- **不要**再引入任何「為了讓某個數字對上」的常數、偏移或校準項。**v23 的合成值本來就會不同，
  那是預期的**——只有 zone 分量該與 v23 相等。
- 其餘照原任務書：不 bump v23、不開 enum、`-j2`／`--parallel 2`、不 push、不動 `design/`、
  不碰 RulesetPatcher／`add_faction`／`inject/`。

## 回報

`wf/inbox/m10-3a-fix1-complete.md`（≤8KB 繁中，首行 `DONE`/`BLOCKED`）：4 條驗收逐項證據
（負向控制前後輸出必附）、uid 改法、**格式變更清單增修**（給整合輪定 v24）、
以及**前輪「無法重建清單」兩條照抄過來**（那份清單我要留著）。

**最重要**：這一輪的重點不是讓數字好看，是**讓每個數字在對應的東西壞掉時真的會動**。
如果你發現定序 fold 之後某個分量其實從來不影響結果，**如實回報**，不要加東西去補。
