# Site 生命週期與時間補算

> 承接 [interface-world-mid.md](interface-world-mid.md)。
> 那份講「兩層狀態怎麼對應」，這份講「Site 消失又回來時怎麼不出錯」。
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

## 狀態機 = LOD 等級

Site 的生命週期狀態**就是** [observer.md](observer.md) 的四個 LOD 等級，不另立一套詞彙：

```
L_ABSENT ⇄ L_FROZEN ⇄ L_COARSE ⇄ L_FULL
        （升級走 rematerialize，降到最底一階時寫盤）
```

| 等級 | 記憶體 | 每旬做什麼 |
|---|---|---|
| `L_ABSENT` | 0（磁碟上留 SiteDigest） | 不跑，由 Region 的近似公式代表 |
| `L_FROZEN` | SiteDigest 常駐 | 不跑，被查詢時即時投影出答案 |
| `L_COARSE` | 骨架 + 持久層 | 歸約一次、跑聚合統計、接受 Region 事件 |
| `L_FULL` | 完整三層 | 逐小時推進，個別實體都算 |

**同時可以有很多個 `L_FULL` 的 Site。** 上限來自預算，不是「一次一個」的規則——
玩家、每個 mark 過的對象、每個進行中的大事件都各自撐起一片高解析度區域。

## 升降級的觸發

等級由**場強**決定，不是由一串 if-else 決定：

```
score(site) = max over observers of ( strength − travel_cost )
```

| 轉換 | 觸發 |
|---|---|
| 升級 | 場強跨過該級門檻。玩家進入、mark、大事件爆發，都是透過**產生 observer** 來抬高場強 |
| 降級 | 場強跌破門檻（observer 消失或走遠），**或**預算超支時逐出分數最低者 |
| 落到 `L_ABSENT` | 進 `L_FROZEN` 後仍連續 `N` 旬無場強、無進行中大事件、無未完成建造 |
| `pinned` | 玩家所在、進行中的大事件——**永不降級**，不受預算約束 |

門檻與 `N` 是可調參數。荒野 Site 用更短的 `N`（初值 2），因為它們幾乎沒有持久層；
城市用較長的 `N`（初值 12，約三分之一年）。

**降級與逐出只在回合結算的尾端執行**，絕不在模擬途中
（[principles.md](principles.md) 原則七）——避免懸置指標與迭代器失效。

## SiteDigest：卸載時留下什麼

卸載時把 Site 壓成一份小結構，掛在 Region tile 上：

```cpp
struct SiteDigest {
    Tick               unload_tick;    // 卸載時的全局時鐘
    uint64_t           site_seed;      // 重算程序層用
    uint32_t           skeleton_hash;  // 慢變數指紋，見下方「骨架失效」
    std::vector<PersistentObject> objects;   // 持久層（建築、通道、具名 NPC、已開寶箱旗標）
    std::vector<InProgress>       pending;   // 未完成的建造／研究，含剩餘時數
    FlagSet            story_flags;    // 劇情旗標
};
```

典型大小：一座大城幾百個物件，幾 KB；荒野通常是空的。
**程序層與易失層一個位元都不存。**

## 重載：三步

```
rematerialize(region_tile_now, digest, now) -> Site
```

> ⚠ **`load` 不是 `rematerialize`**（M2.3 實測踩到）。
> 存檔的 `load` 只解碼**持久層**——那是白名單決定的
> （見 [zone-save-format.md](zone-save-format.md)），程序層本來就不在裡面。
> 所以 `load` 回來的 Site **程序層是空的**。
>
> 這件事的危險不在於它會壞，而在於**它不會壞得很明顯**：
> 拿 `load` 當重新展開去做往返測試，兩次的程序層都是空的，**測試照樣通過**。
> 重新展開必須走完整的三步，`rematerialize` 與 `load` 要是**兩個不同的入口**。

1. **重建骨架** — `build_skeleton(slow_vars_now, digest.site_seed)`。
   慢變數幾乎沒變，所以骨架和卸載前一模一樣。
2. **重新填充** — `populate(skeleton, fast_vars_now)`。
   這一步自動反映了這 N 旬 Region 層的所有演化：人口漲了，程序層的無名 NPC 就多了；
   建設等級升了，程序層的次要建築就密了。**這裡不需要任何補算。**
3. **疊回持久層並補算** — 把 `digest.objects` 放回原座標，然後只對持久層做 elapsed 推進。

### 關鍵洞見

> **需要「補算」的只有持久層，因為程序層是從「現在」的 Region 狀態算出來的。**

Region 從來沒停過。人口成長、資源累積、勢力更迭、季節推移——這 N 旬的帳，
Region 早就結清了。重載時我們拿的是**當下**的 Region 狀態，不是卸載當時的。
所以絕大部分的「時間流逝」是**免費**的，不必模擬。

這就是為什麼「Region 是權威」那條原則值得付出代價來守：
它把最可怕的「離線模擬」問題縮小到只剩幾十個持久物件。

## 持久層補算

`elapsed = now − digest.unload_tick`（秒）。對持久層做**閉式**推進，不逐回合迭代：

| 對象 | 補算方式 |
|---|---|
| 未完成建造 | `remaining -= elapsed`；`<= 0` 就完成，並回填一次歸約 |
| 建築狀態 | 依當下快變數重算 active/idle/derelict/ruined（不需要 elapsed） |
| 建築老化 | 若期間該格 `owner` 無人或 `damage` 高，依 elapsed 推進傾頹等級，有上限 |
| 具名 NPC | 年齡 += elapsed；依 Region 事件記錄判定是否已死亡／遷移 |
| 隨機事件 | 用 `Poisson(λ × elapsed)` 抽出次數，種子取 `hash(site_seed, unload_tick)`，保證確定性 |
| 資源存量 | **不補算**——存量早就歸約進 Region 的 `city.stock`，直接讀回來 |

### 飽和上限

任何依 elapsed 推進的量都必須有上限。離開 500 旬回來，
建築不能傾頹一萬次、事件不能抽出三百件。實作上對 `elapsed` 取
`min(elapsed, cap)`，`cap` 依量的性質設（老化 24 旬封頂、事件 6 旬封頂）。

沒有上限的線性外推是離線模擬類系統最常見的爆炸點，這條是硬性要求。

## 骨架失效

罕見但必須處理：卸載期間 Region tile 的**慢變數**被改了
（地震把丘陵變平原、玩家鋪了一條新道路、洪水改道）。

作法：`skeleton_hash` 記錄卸載當時慢變數的指紋。重載時比對：

- **相同** → 骨架不變，持久層座標直接沿用（絕大多數情況）
- **不同** → 執行**遷移（migration）**：
  1. 用新慢變數重建骨架
  2. 逐一檢查持久層物件的座標在新骨架上是否仍合法
  3. 合法 → 保留；不合法 → 就近搬到最近的合法格；找不到 → 標記為毀於災變並產生一則事件

遷移一定會產生玩家看得到的變化，所以它必須**伴隨敘事**（「山崩摧毀了東側的三座工坊」），
而不是靜靜地把東西挪走。

## 一致性驗證

**獨立一檔** → [interface-verification.md](interface-verification.md)。
M4 的完成判準、七條測試表、「卸載等價」為何刻意允許 10% 誤差，以及
**每條測試都要有負向控制**（沒有偵測力的通過不算通過）都在那裡。

## 待細化

- `N`、各飽和上限、Poisson 的 λ 的實際數值
- 持久層物件的完整型別清單與序列化格式

> 兩項已完成，移出待細化：
> 「大事件如何鎖住 Site 不被卸載」→ `pinned` 旗標（見上方升降級表）；
> 「L2↔L3 沿用同一套機制的差異點」→ [lowmap.md](lowmap.md)。
