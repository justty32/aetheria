# 規則檔佈局與 def 引用

> 從 [definitions.md](definitions.md) 拆出。那份講 **def 是什麼、為什麼不用 enum、
> tile 存什麼、存檔存什麼**，這份只回答兩件事：
> **規則檔在磁碟上長什麼樣，以及 def 之間怎麼互相指。**
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 資料檔佈局

```
data/
  terrain.toml    relief.toml    feature.toml    edges.toml
  buildings.toml  units.toml     zone_kinds.toml  events.toml
mods/<name>/data/...             ← 疊加：同 id 覆寫、新 id 新增
```

一類一檔（不是單一大檔）。理由：幾百到幾千筆 def 塞一個檔難手寫、難 diff、載入慢，
而且分檔天然就是擴展包的疊加單位。

## def 之間的引用

`BuildingDef` 要指向「解鎖它的科技」、`UnitDef` 要指向「需要的資源」——def 本質上是一張圖。

- 資料檔裡寫**字串 id**（人可讀、可 diff、跨檔案安全）
- **載入期解析成下標**存進 struct（執行期 O(1)）
- 解析不到 → fail-fast

`FeatureDef.required_terrain`（可選）是目前唯一的跨 def 引用，語意是**生成期約束**
（森林不長在海上、礦脈要有山地），不是執行期不變式——載入期解析不到即 fail-fast，
但已存在的存檔不因資料檔改動而失效（那由字串 id 重映射負責）。

**id 命名空間全域唯一**，用型別前綴區分：`"terrain.grassland"`、`"building.smithy"`。
理由：之後一定會出現「一個欄位要指向不特定型別的 def」（生產佇列可以是單位／建築／專案），
每型別各自從 1 開始編號會讓這種異質引用需要額外帶一個型別欄位。
全域唯一字串 id 天然可行，代價只是作者要避免撞號——而載入期會偵測撞號並 throw。
