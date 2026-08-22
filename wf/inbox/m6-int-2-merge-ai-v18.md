# 任務書 M6-INT-2 — 併入 M6-INT-1，版本號收斂到 v18

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**必讀設計**：[`zone-save-history.md`](../../design/zone-save-history.md)
**必讀協定**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md) ← **新的，這輪開始用**
**基準**：main（已含 M6.6c 的 v17）

---

## 又撞版本號了，這次是我第二次犯同樣的錯

| 分支 | `kSaveFormatVersion` | 內容 |
|---|---|---|
| main（M6.6c 已併入） | **17** | Site payload 加了治安四因子、NPC／地城旗標 |
| `m6-int1-wt` | **17** | zone 格式加了 `NamedFateLedger` + 勢力 AI 三類 |

⚠ 兩者都叫 v17 但位元流不同。**裁定：合併後是 v18，內含全部。**
理由與上一輪相同：同號不同流會製造**讀得進去但錯位**，
那正是版本欄位要把「靜默讀壞」變成「大聲拒讀」所防的事。

**這是我畫並行造成的，不是你們的錯**——兩路各自看到的上一版都是 16。

## 要做的

1. 在本 worktree 執行 `git merge --no-ff m6-int1-wt`
2. 把 `kSaveFormatVersion` 收斂到 **18**
3. **不留任何 v17 解碼路徑**（兩個 v17 都沒穩定存在過）。
   v14/v15 的低階 fixture 解碼**可以保留**（INT-1 用它們驗「缺席 ≠ 預設值」，那有價值），
   但公開入口 `FileZoneStore` 只接受 v18。
4. 兩邊的缺席斷言**全部保留**，合併後必須同時涵蓋 **七類**：
   `NamedFateLedger`、`FactionTruth`、`KnowledgeRecord`、`FactionMindState`、
   `SiteOrderState`、`PersistentNamedNpc`、`PersistentDungeon`

⚠ **七類一個都不能漏，而且要各自有獨立斷言**，不是一句「全部缺席」帶過。

## 驗收

| 判準 | 怎麼量 |
|---|---|
| 版本是 v18 | `grep` 不到殘留的 v17 解碼路徑 |
| 舊檔拒讀 | v17 與 v15 都大聲拒讀，附兩個錯誤訊息 |
| 七類缺席斷言 | 附七個斷言的檔案與行號 |
| 七個觀測 scalar 的雜湊偵測力沒退化 | M6.6c 那條測試仍綠，附數字 |
| 全套綠 | `ctest` 數字 |

**負向控制**：七類中**任選兩類**，把它的缺席斷言拿掉，
確認該類的漏失**再也抓不到**——證明七個斷言各自有效、不是互相頂替。附實測。

## 不要做的事

| 不要 | 理由 |
|---|---|
| 沿用 v17 | 兩個不同的 v17 會靜默讀壞 |
| 做遷移 | 政策是拒讀 |
| 改演算法 | 這輪只做整合 |
| 改 `design/`（v18 沿革寫進回報，我來加） | 我裁定 |

## 回報

`wf/inbox/m6-int-2-merge-ai-v18-complete.md`：衝突怎麼解、兩個拒讀錯誤訊息、
七個斷言的行號、負向控制實測、`ctest` 數字、現有測試證不了的事。

## 規約

- `cmake --build build --parallel 2`｜不准 fan-out 自我審查子 agent｜不要改 `design/`｜
  不要 push｜繁體中文、每份文件 ≤ 8 KB
