# Zone 定址：混合方案（定案）

> 從 [zone-model.md](zone-model.md) 拆出。那份講 zone 的**結構與生命週期**，
> 這份只回答一件事：**一個 zone 的位址怎麼算出來。**
> 存檔路徑推導見 [zone-save.md](zone-save.md)；繼承來源見 [medps-relation.md](medps-relation.md)。

**空間 zone 用座標推導，非空間 zone 用單調序號。**
這是 [medps-relation.md](medps-relation.md) 繼承清單第 11 項的定案。

## 位元佈局

```
bits 63-60 : level (4)     0=Root, 1=Region, 2=Site, 3=Local, 15=Detached
level != Detached:
  bits 59-44 : region_id  (16)   哪個大陸
  bits 43-32 : site_x     (12)   在 Region 中的格位
  bits 31-20 : site_y     (12)
  bits 19-10 : local_x    (10)   在 Site 中的格位
  bits  9-0  : local_y    (10)
level == Detached:
  bits 59-0  : 單調序號
```

未使用的層級欄位一律填 0。`ZoneKey(0)` 即 root。

```cpp
constexpr ZoneKey parent_of(ZoneKey k) noexcept;   // 純位元運算，不查表
constexpr ZoneKey child_key(ZoneKey parent, uint32_t x, uint32_t y) noexcept;
constexpr uint32_t local_x_of(ZoneKey k) noexcept; // child 在 parent 的哪一格
```

`parent` 因此**不必存進 `Zone`**——從 key 算得出來。這是座標定址的紅利之一。

## 為什麼選座標推導（而 medps 選了零語意序號）

medps 在 2026-07-22 放棄了座標語意，改用零語意 `uint64_t` 序號 + `parent` 鏈，
理由是「ToME 短名定址無座標語意也活得很好」。aetheria 走回座標推導，因為：

| 收益 | 說明 |
|---|---|
| **ChildLink 整題免費消失** | 「child 在 parent 地圖的哪一格」直接從 key 讀出。medps 把這題 defer 了（`zone-addressing-lifecycle-design.md` §3.4），aetheria 不必付這筆帳 |
| **投影天然可定址** | [interface-world-mid.md](interface-world-mid.md) 的 `project(region_tile)` 需要「這格的 Site 是誰」——一次位元運算 |
| **接邊的規範化 id 直接可算** | [edge-consistency.md](edge-consistency.md) 的 `canonical_edge_id` 需要兩側的格位，從 key 就有 |
| **不必配發、不必同步** | 沒有 `next_id` 計數器，就沒有「配發後 crash 導致 manifest 與磁碟不一致」的窗口 |
| **`parent` 不必存** | 少一個欄位、少一處可能不一致的來源 |

aetheria 的巢狀是**嚴格且稠密**的（每個 tile 恰好對應一個下層 zone），
這正是座標定址最划算的形狀。medps 的顧慮成立於它沒有落地 ChildLink 的前提下。

## 代價與對策

| 代價 | 對策 |
|---|---|
| **世界尺寸被烘進 key** | Region 與 Site 的尺寸寫進存檔的 manifest，並宣告 **IMMUTABLE**。中途更動會讓所有既有 key 失效——這與 medps 的 `WorldConfig` 是同一個取捨 |
| 欄位寬度定死上限 | 刻意給了餘裕：Region 最大 4096×4096（實際 128×96）、Site 最大 1024×1024（實際 64×64）。上限不會在可預見的未來咬人 |
| 非空間的 zone 無處安放 | 這正是 `Detached` 存在的理由 |

## Detached：非空間 zone

有些 zone 不對應世界上的任何一格：劇情用的口袋空間、傳送門後的異界、夢境、試煉場。
它們用單調序號定址，配發規則同 medps 的 `create_child`（`next_id_++`、0 保留、**永不復用**）。

`parent_of(detached)` 回傳 root。它們的「在哪裡」由玩法層自己記（通常是一個 Portal 實體）。

## Key 是位址，不是存在證明

**任何合法座標都有一個 key**，但只有被具現化過的 zone 才有磁碟檔案。
這正好對應 [interface-lifecycle.md](interface-lifecycle.md) 的 `L_ABSENT`：
位址永遠算得出來，內容按需生成或載入。
存在性檢查 = **磁碟上有沒有那個檔案**，不維護任何清單。

這條與 [zone-model.md](zone-model.md) 的成長軸不變量互為表裡：
**不維護清單**之所以可行，正是因為位址不需要被記住，隨時算得出來。

## 待細化

- `level` 欄位給了 4 bits（16 種），目前用掉 5 種。多出來的值**保留不用**，
  不要拿去塞旗標——那會讓 `parent_of` 的位元運算長出分支。
- Detached 序號的存放位置（manifest 的哪個欄位）見 [zone-save.md](zone-save.md)。
