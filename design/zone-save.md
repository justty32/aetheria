# 存檔與序列化

> zone 怎麼落到磁碟、怎麼讀回來、格式變動怎麼辦。
> 結構見 [zone-model.md](zone-model.md)；繼承來源見 [medps-relation.md](medps-relation.md)。

## 選型：cereal + EnTT snapshot

繼承 medps 的既有選型，理由是它已經跑通且有測試（`medps/docs/references/cereal_tutorial.md`）：

| 用途 | 工具 |
|---|---|
| 實體與 component | EnTT `snapshot` / `snapshot_loader` |
| 位元組編碼 | cereal `PortableBinary`（跨平台位元組序一致） |
| 壓縮 | zstd（tile 陣列壓得很好） |
| 規則檔（可手寫） | TOML，**不進存檔**（見 [definitions.md](definitions.md)） |

**不用通用序列化框架的反射魔法。** 格式要能自己控版本、自己看懂位元組流。

## 一個 zone 一個檔，路徑由 key 推導

```
saves/<slot>/
  manifest.bin              世界級 metainfo
  root.bin                  root zone
  a3/a3f2000100200000.bin   分桶：hex 前兩碼一層目錄
```

- **路徑用 key 算出來，不維護任何清單。**
  「這個 zone 存不存在？」＝ `exists(path(key))`。
- **目錄分桶現在就做。** 現在是改 `path()` 三行；等到有數萬個檔案再改，
  就是搬檔案 + 寫遷移工具。pre-v1 沒有存檔資產，這筆帳現在付最便宜——
  這條論證直接取自 medps `zone-addressing-lifecycle-design.md` §3.3。

## zone 檔的兩塊

`Zone` 的地圖不走 ECS，所以一個 zone 檔是兩塊接在同一個 stream 上：

```
[檔頭]  magic + format_version + key + last_saved_tick
[第一塊] layers（各 z 的 TileGrid，SoA 逐欄寫）+ persistence 旗標
[第二塊] registry 的 EnTT snapshot（依 AllComponents 展開）
```

**檔頭帶 magic 與 version**。medps 早期把版本欄位全滅、靠「格式一變就刪存檔」，
aetheria 不跟這條——版本欄位把「靜默讀壞」變成「大聲拒讀」，
而 `AllComponents` 清單一動就會默默改變位元組流，這正是它值得先行的理由。

## `AllComponents`：唯一清單

```cpp
using AllComponents = entt::type_list<
    ZoneMeta, Position, /* ... */
    NewComponent           // ← 永遠加在最後
>;
```

**新增 component 必須同步登記，否則存檔會漏掉它。**
順序即位元流順序，**永遠加在最後、不要插中間**，否則舊存檔讀取時資料錯位。

這條要進 `AGENTS.md` 的鐵律——medps 就是這樣做的，因為它是最容易忘、
後果又最安靜的一條。

### `orphans()` 陷阱

EnTT 的 `snapshot_loader::orphans()` 會刪掉「沒有任何 component」的 entity。
一個只帶**未登記** component 的 entity 會整個消失，而且**不會報錯**。

對策：每個 zone 至少有一個帶 `ZoneMeta` 的 placeholder entity（保證身分與存活），
且測試要比對載入前後的 entity 數。

## 存字串 id，不存下標

**存檔絕不存 def 的執行期下標**——下標會因資料檔改動而整體位移。

作法：存檔開頭寫一份 id 表（`下標 → 字串 id`），主體用下標，
讀檔時用當前 `Ruleset` 重映射。詳見 [definitions.md](definitions.md)。

## 跨 zone 引用

`entt::entity` 只在自己的 registry 內有意義。跨 zone 一律用：

```cpp
struct StableId { uint64_t uid; };                    // opt-in component
struct EntityRef { ZoneKey zone; uint64_t uid; };     // {0,0} = null
```

- `uid` 由 `ZoneManager` 配發，計數器存在 manifest。
- `StableId` **只掛具名／被 mark 的實體**，不是每個雜兵都有——
  這與 [unique-objects.md](unique-objects.md) 的獨特／標籤二分是同一條線。
- 解引用**兩段皆可失敗**且失敗是預期結果：
  `get(ref.zone)` 回 `nullptr` ＝ zone 未載入（呼叫端決定要不要 load）；
  `find_uid(uid)` 回 `entt::null` ＝ 已死亡。
- `uid_index` 是**執行期**結構，load 後由 `view<StableId>` 重建。
  重建撞號＝存檔損毀 → throw 附 zone key + uid。

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
| **entity 數守恆** | 載入前後 entity 數相同（`orphans()` 陷阱的哨兵） |
| **component 覆蓋** | 每個登記在 `AllComponents` 的型別都有 round-trip 測試 |
| **懸空 def id** | 用改過的規則檔讀舊存檔 → 明確報錯，不靜默 |
| **manifest 撕裂** | 人工截斷 manifest → 拒絕開檔，不覆寫 |
| **key 不符** | 人工改檔內 key → throw 附兩個 key |

## 待細化

- `TileGrid` 各層的欄位與壓縮策略
- `StatusDigest`／`SiteDigest` 的實際序列化形式
- 多存檔槽（目前單槽；多槽＝目錄複製，defer）
- 背景存檔（目前同步；痛了再說）
- 「清理很久沒訪問且不重要的 zone 檔」——medps 記過的需求，aetheria 同樣需要
