# 任務書 M5.18 — 地形生成的外部對照（**只產出判斷，交給未來的地形 session**）

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**本輪只寫一份回報。不要改任何 `core/`、`data/`、`tests/`、`design/`、`cmake/`。**
（M5.15 在做歸約、M5.16 在調 FOV 預算、M5.17 在評估 libtcod 的 FOV。）

---

## 定位

使用者裁定**地形生成之後單開一個 session 處理**，本 session 已經停手、只留了接口。
這輪**不是要動地形**，是要**替那個 session 先把外部作法查清楚**——
它接手時第一件事就不必再花一輪讀別人的程式碼。

⚠ **一行地形程式碼都不要改。** 產出就是一份判斷。

## 已知的病灶（不要重新診斷，直接拿去對照）

一行指令可重出：

```sh
./build/aetheria_sim gen terrain-metrics --seed 515151
```

```
moisture_histogram  min=8833 median=61449 p95=65535 max=65535
  counts=28,67,110,73,104,124,86,94,139,136,137,149,179,213,238,1809
                                                                 ↑ 最後一格佔 49%
moisture_saturated_ratio=0.418     terrain_histogram swamp=1823（saturated=1541）
```

**濕度削頂 → swamp 規則在所有飽和格上勝出 → 吃掉半張圖。**

M5.9 已經對照過 redblobgames（結論在 `wf/inbox/m5-9-redblobgames-study-complete.md`），
**這輪是看「實際出貨的遊戲」怎麼做**，不要重複 M5.9 已經寫過的內容。

## 要看的：**分析目錄**，不是原始碼

⚠ 使用者指名：**`~/repo/pas/analysis/` 與 `~/repo/game_dev/analysis/` 才是該看的**——
那裡是已經消化過的分析，比啃原始碼快得多。M5.9 引用過的
`~/repo/pas/analysis/World-Generator/architecture/Level3-地形與氣候演算法.md` 就在那裡。

底下與本題相關的至少有：`generator`、`freeciv`、`cdda`、`luanti`、
`cultivation-world-simulator`、`brogue-ce`、`dcss`。**自己掃過目錄挑相關的**，
原始碼只在分析講不清楚時才回頭去翻。

另外 `~/repo/game_dev/python-tcod` 有 libtcod 的 **heightmap** 一整組 API
（中點位移、侵蝕、noise 疊加、normalize／rank remap）。
⚠ **特別注意有沒有現成的「分布重整」**——那正是我們濕度削頂缺的東西。

## 三個要回答的

1. **他們怎麼避免「某個場削頂／某條規則吃掉整張圖」？**
   有沒有做 rank remap／百分位映射／直方圖等化？貼**檔案路徑與行號**。
2. **溫度與濕度是怎麼來的？** 獨立噪聲、緯度曲線、風場、還是距水體距離？
   ⚠ 我們現在是**風掃描 + 雨影**（`core/worldgen/stage_climate.cpp:87-132`），
   比多數作品複雜——**如果他們用更簡單的作法卻得到更好的圖，那件事值得知道。**
3. **libtcod 的 heightmap 值不值得借？**
   ⚠ **決定性是硬條件**：同 seed 同輸出、跨平台一致。做不到就直接淘汰，這條不能妥協。

## 回報

`wf/inbox/m5-18-terrain-references-complete.md`（≤ 8 KB，超過依子題拆檔）：

1. 三個問題的直接答案，每條附路徑行號
2. **排序過的建議清單**，給未來那個 session 當起點——不是「全都可以考慮」
3. 你認為**最可能一招解決濕度削頂**的是哪一個，以及為什麼

## 規約

- 不改任何程式碼｜不准 fan-out 子 agent｜不要改 `design/`｜不要 push｜繁中
- 引用外部一律附**檔案路徑與行號**，不要憑印象
