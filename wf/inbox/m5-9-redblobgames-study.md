# 任務書 M5.9 — 讀 redblobgames，對照我們的管線，提出改法（**不寫程式碼**）

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**本輪只產出一份回報。不要改任何 `core/`、`data/`、`tests/`、`design/`。**
（M5.7 在改 `stage_plates.cpp`／`stage_height.cpp`，M5.8 在改 faction 測試。）

---

## 使用者的判決

看過 M5.6 的計分競爭結果後，原話：

> **「還是很怪。」** 並指名 <https://www.redblobgames.com/>

前面幾輪已經修掉了三個結構缺陷（死規則、first-match、門檻落在分布外），
指標都達標了，**但圖還是不對**。這表示問題已經不在那幾條規則，
而在**更上游的高度／濕度場本身怎麼產生**。

## 要讀什麼

Amit Patel 的站上與本題直接相關的幾篇（自己找正確網址，別猜）：

使用者指名了這四個網址，**全部都要讀**：

1. <https://www.redblobgames.com/maps/terrain-from-noise/>
   —— **redistribution**（對高度做 `e^exponent` 重分布來命中目標分布）、
   **island shapes / radial falloff**、terraces。
2. <https://www.redblobgames.com/articles/noise/introduction.html>
   —— 噪聲的頻譜與 octave 到底在做什麼。⚠ **我們的 `gen_noise.h` 已經有 fbm，
   所以重點是「噪聲該作用在哪一步」，不是「有沒有噪聲」。**
3. <http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/>
   —— 尤其是 **noisy edges**（用中點位移把多邊形邊界弄成碎形）。
   **這一招正對著「海岸線太規整太尖銳」。**
   也看他的 **elevation = 距海岸距離**、**moisture = 距淡水距離**——
   ⚠ 那跟我們各自獨立取噪聲是**結構性的不同**，是本輪最重要的對照點。
4. <https://www.redblobgames.com/x/2022-voronoi-maps-tutorial/>
   —— Voronoi 版本的作法，對照我們的 `stage_plates.cpp`。

## 要交什麼

一份對照表，**每一條都要有兩側的證據**：

| 欄位 | 要求 |
|---|---|
| 他的作法 | 附**網址與章節標題**（頁面會變，寫清楚是哪一節） |
| 我們現在的作法 | 附**檔案路徑與行號**（`core/worldgen/stage_*.cpp`），不要憑印象 |
| 差在哪 | 一句話講清楚結構差異 |
| 值不值得改 | 你的判斷 + 理由 |
| 成本 | 會動到哪幾個階段、會不會破壞既有決定性與接邊一致性 |

然後給我一份**排序過的建議清單**：先做哪個、為什麼那個排第一。

⚠ 我要的是**排序**，不是「全都做」。今天已經證明**一輪一個變因**才看得出誰起作用。

## 兩件要特別回答

1. **noisy edges 能不能直接用在我們的板塊邊界上？**
   M5.7 正在試 domain warping 解同一個問題。**這兩招是替代還是互補？**
   如果 noisy edges 更好，就說出來——M5.7 的成果可以被取代，我不心疼。
2. **「elevation = 距海岸距離」跟我們的接邊一致性相容嗎？**
   我們有 `edge-consistency.md` 的降維裁決鏈（角→邊→面），
   而「距海岸距離」是**全域**性質。⚠ 這可能是個真正的衝突，
   如果是，講清楚，不要假裝相容。

## 不要做

不要動任何程式碼。不要「順手試一下」。**這輪的產出就是一份判斷。**

## 回報

`wf/inbox/m5-9-redblobgames-study-complete.md`（≤ 8 KB，超過就依子題拆檔）。

## 規約

- 不准 fan-out 子 agent｜不要改 `design/`｜不要 push｜繁中
- 引用外部結論一律附**網址**；引用本專案一律附**檔案路徑與行號**
