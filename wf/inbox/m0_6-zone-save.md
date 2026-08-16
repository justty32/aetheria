# 信：任務書 M0.6 — zone 落到磁碟

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**必讀設計**：[`design/zone-save.md`](../../design/zone-save.md)（整份）、
[`design/zone-model.md`](../../design/zone-model.md) 的「存取：作用域借用」
**基準**：`b1c824f`（M0.5.1 已通過審閱）

---

## 先說 M0.5.1：通過

`with()` 的 mutable／const 兩版、borrower 限制回傳 `void`、`tick()` 改傳 `Zone&`、
**後門 `tile_count()` 真的移除了**——我 `grep` 過，`core/` 與 `tests/` 都沒有殘留。
`ctest` 21/21 我自己跑的。

`QueuedUnloadKeepsTickBorrowAliveUntilCallbackReturns` 這個測試名字取得好：
它直接把「契約一支撐契約三」這件事寫成了可執行的斷言。

## 這份任務要證明什麼

**M0.5 造了 zone 的生命週期，但它活在記憶體裡，關掉就沒了。M0.6 讓它落地。**

真正要證明的是一句話：**存下去再讀回來，狀態逐位元相同。**
整個 M2／M4（本專案的兩個真正技術風險）都建立在這條上——
「Site 卸載後推進 N 旬再載入，結果與不卸載一致」如果連 round-trip 都不逐位元相同，
後面全部免談。

## 範圍

### 1. `FileZoneStore`

實作 `ZoneStore` 介面的磁碟後端。**`InMemoryZoneStore` 保留**——
它是測試用的快速替身，不要刪。

路徑照 `zone-save.md`：`saves/<slot>/`，一個 zone 一個檔，
**hex 前兩碼分桶**，路徑由 key 推導，不維護任何清單。
**分桶現在就做**，理由在設計文件裡（現在是改三行，之後是寫遷移工具）。

### 2. 檔案格式

三塊：檔頭（magic + format_version + key + last_saved_tick）、
layers、registry 的 EnTT snapshot。工具鏈照設計：
**cereal `PortableBinary` + zstd + EnTT snapshot**。vcpkg 要加 `cereal`、`zstd`。

`PortableBinary` 是刻意選的（跨平台位元組序一致）——**不要換成 `BinaryArchive`**，
那個不可攜，會讓決定論在換機器時破功。

### 3. `AllComponents` 與 `orphans()` 陷阱

`AllComponents` type_list 現在只有 `ZoneMeta`（M0.6 還沒有真的 component）。
但**兩條規矩現在就要立**：

- **新增 component 必須同步登記，且永遠加在最後、不准插中間。**
  順序即位元流順序，插中間會讓舊存檔錯位。
- 每個 zone 至少一個帶 `ZoneMeta` 的 placeholder entity，
  且測試比對載入前後的 entity 數——這是 `orphans()` 陷阱的哨兵。
  EnTT 會**靜默刪掉**只帶未登記 component 的 entity，不報錯。

**把第一條登記進 [`wf/workflows/common/conventions.md`](../workflows/common/conventions.md)。**
（`zone-save.md` 原本寫「這條要進 `AGENTS.md` 的鐵律」，我改主意了：
`AGENTS.md` 是薄路由器，程式碼慣例歸 `conventions.md`。設計文件我會改。）

### 4. manifest 與開檔協定

manifest 欄位照設計。**寫入一律 tmp + rename 原子替換。**

開檔協定三種狀態照設計那張表——特別是「**無 manifest 但有 `.bin` → throw**」。
不得靜默當成新世界然後覆寫舊檔。

## Done when

- [ ] **round-trip 逐位元相同**：存 → 讀 → 狀態雜湊一致（這條是本任務的核心）
- [ ] **entity 數守恆**（`orphans()` 哨兵）
- [ ] `destroy` **同步刪除磁碟檔案**，且刪除後 `load` 回 `false`（不會靜默復活）
- [ ] `load` 後驗**檔內 key ＝ 請求 key**；人工改檔內 key → throw 附兩個 key
- [ ] **人工截斷 manifest → 拒絕開檔**，且**不覆寫**既有檔案（貼證據）
- [ ] **無 manifest 但有 `.bin` → throw**（貼證據）
- [ ] format_version 不符 → throw（**不寫遷移碼**，重寫期政策）
- [ ] 分桶路徑正確：同一個 key 兩次算出同一路徑；不同 key 不撞
- [ ] `FileZoneStore` 與 `InMemoryZoneStore` **通過同一組行為測試**
      （介面契約一致，換後端不改語意）
- [ ] 四個 target 零警告、CTest 全綠、`aetheria_core` 仍零 godot-cpp
- [ ] `aetheria_sim` 能存一棵 zone 樹、結束、重跑、讀回來並印出相同結果
- [ ] `AllComponents` 的規矩已登記進 `conventions.md`
- [ ] commit 到 `main`（不 push）

## 不要做的事

| 不做 | 為什麼 |
|---|---|
| **def id 的字串表重映射** | 需要 `Ruleset`，那要等 `definitions.md` 進 M1。重寫期版本政策是「格式變就刪存檔」，所以這筆現在欠得起 |
| `StableId`／`EntityRef`／`uid_index` | 還沒有跨 zone 引用的東西。manifest 的 `next_entity_uid` 欄位先留著不用 |
| 多存檔槽、背景存檔 | 設計已列 defer |
| `TileGrid` 的真實 SoA 與壓縮策略 | 佔位型別照舊，M1 才換 |
| 任何生成器或玩法 | M1 |

**「三層資料只存中間那層」現在測不了**（還沒有程序層／易失層的東西）。
把 persistence 旗標的位置留出來，但不要為它發明假資料。

## 回信給我

我最想知道的三件事：

1. **round-trip 逐位元相同這條，有沒有哪裡差點做不到？**
   浮點、padding、`std::map` 迭代順序、EnTT 內部排列——**哪一個先咬你？**
   這是決定論的第一次真實壓力測試，我要知道它從哪裡漏。
2. **zstd 壓進去之後，round-trip 的雜湊是比壓縮前還是壓縮後？**
   你選哪個、為什麼。（壓縮是否決定性會影響這條怎麼測。）
3. **`FileZoneStore` 與 `InMemoryZoneStore` 共用同一組測試時，有沒有哪條語意其實對不上？**
   對不上的地方就是 `ZoneStore` 介面沒設計好，直說。

一樣：**你撞到的現實比我的推演可靠。**
