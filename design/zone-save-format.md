# 存檔格式：位元流細節

> 從 [zone-save.md](zone-save.md) 拆出。那份講存檔目錄怎麼運作
> （路徑由 key 推導、manifest、開檔協定、版本政策），這份只回答一件事：
> **存檔的位元流長什麼樣**——序列化選型、component 清單、跨 zone 引用怎麼編碼。
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

⚠ **`PortableBinary` 預設保留 host endian。** 它**讀**得了任一端序，但**寫**出來的
位元組會隨機器而異——「同一份狀態在兩台機器上存出不同的檔案」會讓決定論在跨平台時
無聲破功。**輸出一律明確指定 `LittleEndian()`。**（實作階段撞出來的，不是推演得到的。）

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

這條要登記進 [`wf/workflows/common/conventions.md`](../wf/workflows/common/conventions.md)
的程式碼慣例——medps 就是這樣做的，因為它是最容易忘、後果又最安靜的一條。
（不放 `AGENTS.md`：那份是薄路由器，durable 的程式碼慣例歸 `conventions.md`。）

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

- `uid` 由 `ZoneManager` 配發，計數器存在 [manifest](zone-save.md)。
- `StableId` **只掛具名／被 mark 的實體**，不是每個雜兵都有——
  這與 [unique-objects.md](unique-objects.md) 的獨特／標籤二分是同一條線。
- 解引用**兩段皆可失敗**且失敗是預期結果：
  `get(ref.zone)` 回 `nullptr` ＝ zone 未載入（呼叫端決定要不要 load）；
  `find_uid(uid)` 回 `entt::null` ＝ 已死亡。
- `uid_index` 是**執行期**結構，load 後由 `view<StableId>` 重建。
  重建撞號＝存檔損毀 → throw 附 zone key + uid。

## 待細化

- `TileGrid` 各層的欄位與壓縮策略
- `StatusDigest`／`SiteDigest` 的實際序列化形式
