# 存檔與序列化

> zone 怎麼落到磁碟、怎麼讀回來、格式變動怎麼辦。
> 位元流格式（序列化選型、component 清單、跨 zone 引用編碼）見 [zone-save-format.md](zone-save-format.md)。
> 結構見 [zone-model.md](zone-model.md)；繼承來源見 [medps-inheritance.md](medps-inheritance.md)。

## 一個 zone 一個檔，路徑由 key 推導

```
saves/<slot>/
  manifest.bin              世界級 metainfo
  root.bin                  root zone
  7c/a3f2000100200000.bin   分桶：桶名＝key 混合雜湊的 2 碼（見下），檔名＝完整 key
```

- **路徑用 key 算出來，不維護任何清單。**
  「這個 zone 存不存在？」＝ `exists(path(key))`。
- **目錄分桶現在就做。** 現在是改 `path()` 三行；等到有數萬個檔案再改，
  就是搬檔案 + 寫遷移工具。pre-v1 沒有存檔資產，這筆帳現在付最便宜——
  這條論證取自 medps `zone-addressing-lifecycle-design.md` §3.3。

### ⚠ 桶名取**混合過的雜湊**，不是 key 的高位

medps 的 key 是**零語意流水號**，高位會變，所以「取 hex 前兩碼」分得開。
**aetheria 的 key 是結構化的**（[zone-addressing.md](zone-addressing.md)）：
最高的 hex 碼是 `level`，第二碼是 `region_id` 的高 4 bits——而世界只有 3～5 個 Region。
於是前兩碼實際只有三種值：

| 層 | 全世界用到的桶 |
|---|---|
| Region | `10/` |
| Site | `20/` |
| Local | `30/` |

**兩億個 Local zone 會全部落進 `30/`。** 分桶等於沒做。

這是照抄 medps 規則、沒有重新推導的結果——**而 zone 定址正是本專案刻意不同於 medps 的那一條**。
繼承來的結論，只要它依賴的前提被改掉了，就必須重驗。

**規則**：桶名取 key 經過一次**位元混合**（splitmix64 的 finalizer 之類）後的 2 個 hex 碼，
共 256 個桶。檔名仍是完整的 16 碼 hex key，所以照 key 找檔案、`ls` 一個桶看內容都不受影響。

256 個桶對照設計的痛點門檻（「數萬個檔案」）綽綽有餘：實際檔案數受
[zone-model.md](zone-model.md) 的成長軸不變量約束，只隨**已造訪**的 zone 數成長，
不是隨世界大小成長。真的不夠了再加一層，那正是本節開頭說的那種遷移。

## manifest

```
uint32  format_version
uint64  next_detached_id      // Detached zone 的序號配發
uint64  next_entity_uid
WorldDims dims                // Region / Site 尺寸，IMMUTABLE
uint64  world_seed
Tick    now
```

**絕不放 zone 清單**（違反成長軸不變量）。

寫入一律 **tmp + rename** 原子替換：撕裂的 manifest ＝ 整包存檔打不開，
一行成本消掉一個單點故障。

## 開檔協定

| 目錄狀態 | 行為 |
|---|---|
| 有 manifest | 既有存檔：版本不符 → throw；還原計數器；**必須成功載入 `root.bin`**（缺失 → throw） |
| 無 manifest 但有 `.bin` | **throw**——不得靜默當成新世界然後覆寫舊檔 |
| 目錄乾淨 | 全新世界 |

## fail-fast 套件（繼承 medps §3.10）

| 檢查 | 時機 |
|---|---|
| `load` 後驗檔內 key ＝ 請求 key | 每次載入 |
| manifest 原子寫 | 每次寫入 |
| `destroy` **同步刪除磁碟檔案** | 否則銷毀後 `load` 會靜默復活 |
| 規則檔壞就 throw，絕不回半套 | 載入期 |
| `require` 找不到檔案 → throw；`load` → 回 `false` | 見 [zone-model.md](zone-model.md) |

## 三層資料只存中間那層

呼應 [principles.md](principles.md) 原則三——**存檔裡的一個 Site 通常只有幾 KB**：

| 層 | 進存檔？ |
|---|---|
| 程序層（地形骨架、無名 NPC、裝飾） | ❌ 重算 |
| **持久層**（玩家蓋的、具名 NPC、已開的寶箱、劇情旗標） | ✅ |
| 易失層（當下座標、HP、動畫） | ❌ 重建 |

事件同理：只存主場層的權威狀態，其餘面孔重算
（見 [event-scaling.md](event-scaling.md)）。

## 版本政策

| 階段 | 政策 |
|---|---|
| **重寫期（現在）** | 版本不符一律 fail-fast，**不寫遷移碼**。格式變更＝刪存檔目錄重玩 |
| 首個對外可玩版本起 | 開始寫遷移碼；存檔成為使用者資產的那一刻，這條就翻轉 |

觸發條件明確寫下來，是為了避免「不知不覺就欠下遷移債」。

## 一致性測試

| 測試 | 判準 |
|---|---|
| **round-trip** | 存 → 讀 → 狀態雜湊逐位元相同 |
| **entity 數守恆** | 載入前後 entity 數相同（[`orphans()` 陷阱](zone-save-format.md) 的哨兵） |
| **component 覆蓋** | 每個登記在 [`AllComponents`](zone-save-format.md) 的型別都有 round-trip 測試 |
| **懸空 def id** | 用改過的規則檔讀舊存檔 → 明確報錯，不靜默 |
| **manifest 撕裂** | 人工截斷 manifest → 拒絕開檔，不覆寫 |
| **key 不符** | 人工改檔內 key → throw 附兩個 key |

## 待細化

- 多存檔槽（目前單槽；多槽＝目錄複製，defer）
- 背景存檔（目前同步；痛了再說）
- 「清理很久沒訪問且不重要的 zone 檔」——medps 記過的需求，aetheria 同樣需要
