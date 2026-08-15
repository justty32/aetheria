# 與 medps 的關係

> **重要發現（2026-08-15）：`~/repo/game_dev/medps` 是同一個構想的前一輪，而且走得相當遠。**
> 動 aetheria 的任何基礎設施前，先讀這份。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 重疊程度

medps 的自我定位是「奇幻版太閣立志傳 × 騎馬與砍殺 × 上古卷軸 × 三國志，
純 C++ 模擬 + Godot 4 GDExtension 前端」（`medps/docs/work/design/zone_layers.md:8-9`）。
它同樣是**三層嵌套地圖**、同樣是 **C++ 核心 + Godot 顯示**、同樣用 **EnTT**。

| | medps | aetheria |
|---|---|---|
| 上層 | World 200×200，一格數公里，1 回合 ≈ 1 日 | Region 128×96，一格 8 km，1 回合 = 1 旬 |
| 中層 | Region 15×15，數十公尺，半即時／WeGo | Site 64×64，125 m，1 小時回合 |
| 下層 | Area 250×250，公尺，即時／JRPG | Local 64×64，約 2 m，1 分鐘／戰鬥 6 秒 |
| 時間模型 | 三層**各自不同**的時間模型 | **統一時鐘**（秒），三層只是不同 stride，且 stride 可變（交戰時縮短） |
| 玩法分層 | 上層 4X、下層 RPG（D-16 拍板） | 三層都是回合制策略，日式表現 |
| 序列化 | cereal + EnTT snapshot | 未定 |

差異是真實的（尤其時間模型與中層尺寸），但**基礎設施幾乎完全重疊**。

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
| **zone 定址** | 零語意 `uint64_t` 單調序號 + `parent` 鏈；「child 在 parent 哪一格」（ChildLink/anchor）**defer 未實作** | **已定案：混合方案**——空間 zone 的 `ZoneKey` 由 `(level, region_id, x, y)` 推導，非空間 zone 用序號。見 [zone-model.md](zone-model.md) | aetheria 的巢狀是嚴格且稠密的（每個 tile 恰好對應一個下層 zone），座標推導讓 ChildLink 這整題**免費消失**，也讓 [interface-world-mid.md](interface-world-mid.md) 的投影天然可定址。medps 放棄它的理由是「ToME 短名定址無座標語意也活得很好」，但那是在 ChildLink 尚未落地時說的 |
| **卸載策略** | LRU + `pinned` | **場強 + `pinned`**（機制照抄，把 `last_touch` 換成 observer 分數） | 逐出「最不重要的」比逐出「最久沒碰的」更貼合玩法，見 [observer.md](observer.md) |
| **離線補算** | defer（被逐出的 zone 就是凍結） | **核心機制之一**，見 [interface-lifecycle.md](interface-lifecycle.md) | aetheria 的中層生命週期是設計前提，不是延後項 |
| **模擬 LOD** | 另案未動（tome4 §2-G 能量制） | **三條軸都做**：空間（observer）+ 實體（significance）+ 事件（event scaling） | 這是 aetheria 的核心玩法機制，不是最佳化 |
| **時間模型** | 三層各自不同 | 單一時鐘、三種 stride | 避免平行時間線，見 [outline.md](outline.md) |

## 拍板：獨立但逐條繼承（2026-08-15）

> **使用者裁定：aetheria 獨立重寫，但逐條繼承 medps 的已拍板決策。**

考慮過但未採用的另外兩個取向，保留為決策紀錄：

| 取向 | 為什麼沒選 |
|---|---|
| ① **接班**（把 `gcore/` 搬過來） | 最快，但要接受 medps 已落地的形狀（零語意 zone id、現有 Tile 寬度）。aetheria 的三個核心機制（統一時鐘、觀察點 LOD、事件縮放）在 medps 都是 defer 或另案，而它們會反過來影響基礎設施的形狀 |
| ③ **放棄 aetheria，回 medps** | medps 的定位（太閣／騎砍／三國志、上層 4X 下層 RPG）與 aetheria（日式表現、三層皆回合制、觀察點與重要性）不同，硬塞會讓 medps 的既有拍板全部重議 |

**這條裁定的含意**：
「繼承清單」是硬性交付物，不是參考讀物。M0 開工前必須逐條核對完。
而「刻意不同」的那幾條也必須逐條寫下理由——兩個 repo 會漂移，
但漂移的每一處都要是**有意識的選擇**，不是忘了看。

## 繼承核對清單

M0 開工前必須逐條完成。每條的判準是「aetheria 有一份文件寫下了它自己的結論，
且該結論明確標示是繼承還是刻意不同」。

| # | medps 決策 | aetheria 落點 | 狀態 |
|---|---|---|---|
| 1 | 方格 4 鄰接 | [worldmap.md](worldmap.md) | ✅ 繼承 |
| 2 | def 進不可變 Ruleset、不進存檔、id→下標索引 | [definitions.md](definitions.md) | ✅ 繼承 |
| 3 | 垂直層收在同一 zone 的 z 鍵 | [outline.md](outline.md) | ✅ 繼承 |
| 4 | 成長軸不變量 | [observer.md](observer.md) | ✅ 繼承 |
| 5 | 逐出只在 tick 尾端 + `pinned` 第二道防線 | [interface-lifecycle.md](interface-lifecycle.md)、[event-scaling.md](event-scaling.md) | ✅ 繼承（LRU 換成場強） |
| 6 | 跨 zone 引用用 zone id + 穩定 uid，解引用可失敗 | [events.md](events.md) 的 `participants` | ✅ 繼承 |
| 7 | 一切皆 zone、root 永駐 | [zone-model.md](zone-model.md) | ✅ 繼承 |
| 8 | 三條執行期契約（tick 重入禁令、單槽活儲存、`Zone*` 不跨 tick） | [zone-model.md](zone-model.md) | ✅ 繼承 |
| 9 | fail-fast 套件（load 驗 id、manifest 原子寫、destroy 刪檔） | [zone-save.md](zone-save.md) | ✅ 繼承，**再加 `require`／`load` 分流**（medps 把這題 defer 了） |
| 10 | 序列化選型（cereal + EnTT snapshot、`AllComponents` 單一清單） | [zone-save.md](zone-save.md) | ✅ 繼承，含 `orphans()` 陷阱的對策 |
| 11 | zone 定址：零語意序號 vs 座標推導 | [zone-model.md](zone-model.md) | ✅ **已定案：混合方案** |

**清單已全數完成。** 兩處刻意不同，理由寫在 [zone-model.md](zone-model.md)：

- **定址走座標推導**（Detached zone 才用序號）。收益是 medps defer 掉的
  ChildLink／anchor 整題免費消失，且投影與接邊的規範化 id 直接可算。
  代價是世界尺寸烘進 key——與 medps 的 `WorldConfig` 是同一個取捨。
- **存檔帶 magic + format_version**。medps 重寫期把版本欄位全滅，
  aetheria 不跟——`AllComponents` 一動就默默改變位元組流，
  版本欄位把「靜默讀壞」變成「大聲拒讀」。

## 補讀清單

尚未讀完的 medps 材料，動對應主題前補上：

- [ ] `medps/workflows/specs/world-zone-subclass-design.md` — Zone 子類如何表達各層的專屬行為
- [ ] `medps/docs/references/entt_tutorial.md` — EnTT 用法與陷阱（`orphans()` 吞實體那條）
- [ ] `medps/docs/references/cereal_tutorial.md` — 序列化
- [ ] `medps/workflows/roadmap/fantasy-civ6.md` 的 §2 系統盤點與 §4 相依鏈（本次只讀了 §5 拍板點）
