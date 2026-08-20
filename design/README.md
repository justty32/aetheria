# 設計文件索引

**每份文件單一檔案、上限 8 KB**；超過就依子題拆成多個單檔，不要拆成資料夾。繁體中文。

## 先讀（依序）

1. [outline.md](outline.md) — 全局常數：三層命名、尺度、時間、曆法
2. [principles.md](principles.md) — **八條原則。其餘文件都只是它們的展開**
3. [glossary.md](glossary.md) — 術語表。**用詞衝突以此為準**（注意「三層」有三個意思）
4. [medps-relation.md](medps-relation.md) — medps 是同一構想的前一輪，動基礎設施前必讀

## 導讀主線

| 想知道 | 順著讀 |
|---|---|
| 遊戲長什麼樣 | `outline` → `worldmap` → `midmap` → `lowmap` |
| 程式怎麼分 | `tech-stack` → `cpp-conventions` → `zone-model` → `zone-addressing` → `definitions` |
| 最難的地方 | `interface-world-mid` → `interface-lifecycle`（對應 M2、M4） |
| 世界怎麼在有限算力下活著 | `observer` → `significance` → `events` |
| 世界怎麼長出來 | `gen-pipeline` → `edge-consistency` → 各層生成器 |
| 玩起來什麼感覺 | `player-residence` → `combat-scaling` |

## 全部文件

**獨立一檔** → [INDEX.md](INDEX.md)。分類完整清單在那裡。

## 尚未規劃

**設計面的主要項目已全部涵蓋。** 剩下的都是刻意擱置，或本質上屬於實作／內容製作：

| 主題 | 狀態 |
|---|---|
| mark 與獨特物件的細節 | 使用者裁定**先擱置**，等縮放機制穩定後回頭定 |
| root 的成長軸 | 使用者裁定**先擱置**（過早優化） |
| 同層近距離事件的快速路徑 | 已知缺口，刻意留到實作撞到再補（[lowmap-streaming.md](lowmap-streaming.md) 末段） |
| **所有數值** | 各文件的「待細化」幾乎都是數值。要靠**實作 + 校準 + 實測**決定 |
| 內容製作 | 神話、法術清單、建築表、任務庫……屬內容，不屬設計 |

**已進入實作。** M0 骨架已落地（見 [build.md](build.md)）；
在辦的任務書在 `../wf/inbox/`，辦完的在 `../wf/inbox/done/`。

## 寫文件的規約

- 每份文件開頭寫清楚上層是誰、負責回答什麼問題，並指向 `principles.md` 與 `glossary.md`。
- 定死的常數只寫在一個地方（多半是 `outline.md`），其餘引用而不複述。
- 尚未決定的寫進末尾「待細化」，不要假裝已經決定。
- 引用外部專案的結論附**檔案路徑（可附行號）**，不要憑印象。
