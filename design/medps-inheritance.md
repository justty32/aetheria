# medps 繼承細節：逐條比對

> 從 [medps-relation.md](medps-relation.md) 拆出。那份講 medps 是什麼、
> aetheria 與它的取向裁定、11 條核對清單的結果、兩處刻意不同的結論；
> 這份只回答一件事：**每條決策 medps 怎麼做、aetheria 怎麼做、為什麼**——
> 已拍板應直接繼承的決策表，以及 aetheria 刻意不同的比對表。
> 核對清單本體與最終結論仍在 [medps-relation.md](medps-relation.md)。

## medps 已拍板、aetheria 應直接繼承的決策

這些不要重新發明，去讀 medps 的原始 spec：

| 決策 | 結論 | 出處 |
|---|---|---|
| **格網拓樸** | 方格、4 鄰接 | roadmap D-1（2026-07-25 拍板） |
| **def 系統** | def 全進不可變 `Ruleset`、**不進存檔**、載入期建 id→下標索引 | roadmap D-2（已落地）→ 見 [definitions.md](definitions.md) |
| **一切皆 zone** | 各層共用同一套 `Zone` + `ZoneManager`，沒有獨立的「世界地圖系統」 | `zone_layers.md:36-42` |
| **root 永駐** | id=0 的 root zone 放跨 zone 的全局實體，永不卸載、不參與一般 tick | `zone_streaming_architecture.md:21` |
| **成長軸不變量** | 持久／常駐結構只能隨「已載入 zone 數」或「已造訪 zone 數」成長，**絕不隨世界總 zone 數** | `zone-addressing-lifecycle-design.md:138` |
| **垂直層收在同一 zone** | 地下／地面／天空是 `Zone::layers` 的 z 鍵，**不是不同 zone** | `zone_layers.md:41-42` |
| **三條執行期契約** | tick 內禁止 zone 結構性變更；存檔目錄＝單槽活儲存；`Zone*` 不跨 tick 持有 | 同上 §3.11-3.13 |
| **跨 registry 不存 entity** | 跨 zone 引用存 zone id + 穩定 uid，解引用**兩段皆可失敗**且失敗是預期結果 | 同上 §3.7 |
| **fail-fast 套件** | load 驗檔內 id、manifest 原子寫、destroy 同步刪檔、規則檔壞就 throw | 同上 §3.10 |

## aetheria 刻意不同的地方

| 題目 | medps | aetheria | 理由 |
|---|---|---|---|
| **zone 定址** | 零語意 `uint64_t` 單調序號 + `parent` 鏈；「child 在 parent 哪一格」（ChildLink/anchor）**defer 未實作** | **已定案：混合方案**——空間 zone 的 `ZoneKey` 由 `(level, region_id, x, y)` 推導，非空間 zone 用序號。見 [zone-addressing.md](zone-addressing.md) | aetheria 的巢狀是嚴格且稠密的（每個 tile 恰好對應一個下層 zone），座標推導讓 ChildLink 這整題**免費消失**，也讓 [interface-world-mid.md](interface-world-mid.md) 的投影天然可定址。medps 放棄它的理由是「ToME 短名定址無座標語意也活得很好」，但那是在 ChildLink 尚未落地時說的 |
| **卸載策略** | LRU + `pinned` | **場強 + `pinned`**（機制照抄，把 `last_touch` 換成 observer 分數） | 逐出「最不重要的」比逐出「最久沒碰的」更貼合玩法，見 [observer.md](observer.md) |
| **離線補算** | defer（被逐出的 zone 就是凍結） | **核心機制之一**，見 [interface-lifecycle.md](interface-lifecycle.md) | aetheria 的中層生命週期是設計前提，不是延後項 |
| **模擬 LOD** | 另案未動（tome4 §2-G 能量制） | **三條軸都做**：空間（observer）+ 實體（significance）+ 事件（event scaling） | 這是 aetheria 的核心玩法機制，不是最佳化 |
| **時間模型** | 三層各自不同 | 單一時鐘、三種 stride | 避免平行時間線，見 [outline.md](outline.md) |
