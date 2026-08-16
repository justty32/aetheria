# 與 medps 的關係

> **重要發現（2026-08-15）：`~/repo/game_dev/medps` 是同一個構想的前一輪，而且走得相當遠。**
> 動 aetheria 的任何基礎設施前，先讀這份。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。
> 逐條比對的細節（已拍板繼承的決策表、刻意不同的比對表）拆到了
> [medps-inheritance.md](medps-inheritance.md)，這份只留關係定位、取向裁定、
> 核對清單結果與刻意不同的結論。

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
| 8 | 三條執行期契約（tick 重入禁令、單槽活儲存、`Zone*` 不跨 tick） | [zone-contracts.md](zone-contracts.md) | ✅ 繼承 |
| 9 | fail-fast 套件（load 驗 id、manifest 原子寫、destroy 刪檔） | [zone-save.md](zone-save.md) | ✅ 繼承，**再加 `require`／`load` 分流**（medps 把這題 defer 了） |
| 10 | 序列化選型（cereal + EnTT snapshot、`AllComponents` 單一清單） | [zone-save-format.md](zone-save-format.md) | ✅ 繼承，含 `orphans()` 陷阱的對策 |
| 11 | zone 定址：零語意序號 vs 座標推導 | [zone-addressing.md](zone-addressing.md) | ✅ **已定案：混合方案** |

**清單已全數完成。** 兩處刻意不同，理由寫在 [zone-addressing.md](zone-addressing.md)：

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
