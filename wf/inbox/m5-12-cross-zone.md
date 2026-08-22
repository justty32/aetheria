# 任務書 M5.12 — 跨 zone 讀與實體搬移：精確化原則一

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**必讀設計**：[`lowmap-streaming.md`](../../design/lowmap-streaming.md)「跨 zone 查詢」與「實體跨 zone 移動」、
[`principles.md`](../../design/principles.md) 原則一
**基準**：M5.11 派出時的 main
**與 M5.11 並行**：那邊在收攏地形常數與留接口。
**⚠ 不要碰 `core/worldgen/`、`data/biomes.toml`、`sim/`。⚠ 使用者在打遊戲，`--parallel 2` 是上限。**

---

## 這輪在回答一個原則問題

原則一說「同層的物件之間永不直接溝通」。但站在 zone 邊界上，視線要看得到隔壁、
尋路要能規劃穿過去——這算不算違反？

> **不算。原則一管的是因果，不是觀看。**
> **看（唯讀、無副作用）可以跨；改（狀態變更）必須經中介。**

而且時機也要分：

| 時機 | 跨 zone **讀** | 跨 zone **寫** |
|---|---|---|
| **生成期** | ❌ **絕不** | ❌ |
| **執行期** | ✅ 可以，但須先確認已載入 | ❌ 一律走事件 |

**生成期絕不跨**的理由是鄰居**可能不存在**——生成時去問鄰居會讓結果取決於生成順序，
也讓「B 從未被生成過」壞掉。整套 `edge-consistency.md` 的前提就是這個。

## ① `peek_tile` / `peek_edge`

```cpp
// 唯讀、可失敗、絕不寫入
std::optional<TileView> peek_tile(ZoneKey, LocalXY) noexcept;
std::optional<EdgeView> peek_edge(ZoneKey, LocalXY, Dir) noexcept;
```

⚠ **回 `optional` 而不是丟例外**，因為「鄰居未載入」是**預期情境**不是錯誤。

退化行為（照設計表，不要自己發明）：

| 情況 | 退化 |
|---|---|
| 視線越過已載入的邊界 | 用鄰居的 digest 給粗略答案，或直接當「看不清」 |
| 尋路目標在未載入的 zone | 用上層 Site 的粗粒度路徑，進入後再細算 |
| 完全沒載入的遠方 | 回「未知」，**不是錯誤** |

### ⚠ 護欄要靠編譯期，不靠自律

> **生成器不准 include 這組函式的 header。用目錄與 target 可見性擋。**

這跟「`aetheria_core` 不碰 godot-cpp」同一個做法，而且你已經有
`CoreIsolation.CompileCommands` 那條測試可以照抄形狀。

**負向控制（編譯期）**：寫一個從生成器 target include 那個 header 的檔案，
**必須編譯失敗**。⚠ 要真的失敗，並把錯誤訊息貼進回報。

## ② `migrate_entity`

```cpp
bool migrate_entity(ZoneKey from, entt::entity e, ZoneKey to, LocalXY at);
```

**單一入口**，因為它必須同時做完這些（照設計表）：

| 職責 | 說明 |
|---|---|
| 目的 zone 必須已載入 | 沒載入就先載入或生成；失敗**回 `false`，不是例外** |
| 複製 component 並在來源刪除 | `entt::entity` 是 per-registry 的 |
| 維護 `uid_index` | 帶 `StableId` 的要在兩邊索引同步 |
| 更新目的 zone 的場強／`touch` | 搬進去讓那裡更值得載入 |
| 空間索引維護 | 未來的格→實體索引掛這裡 |

⚠ **跨 zone 引用一律 `EntityRef{zone, uid}`，絕不存裸 `entt::entity`**——搬移後舊 handle 立刻失效。

## 不要做

FOV、潛行、光照、路線 C 地城、Local→Site 歸約、同層近距離事件的快速路徑
（那條是 `events.md` 已知的設計缺口，**撞到就記進回報，不要自己定形狀**）。

## 驗收

| 判準 | 怎麼量 |
|---|---|
| **生成期不能跨**（編譯期） | 生成器 include 那個 header → **編譯失敗**，貼錯誤訊息 |
| 未載入退化 | 查未載入的 zone → 回 `nullopt`，**不 throw**、不當機 |
| 搬移後舊 handle 失效 | 用舊 `entt::entity` 解引用要抓得到，不能靜默給錯東西 |
| 搬移失敗回 false | 目的 zone 載入不了時，來源實體**必須原封不動**（不能半搬） |
| `uid_index` 兩邊同步 | 搬移後從新 zone 用 uid 找得到、舊 zone 找不到 |
| 決定性 | 同一串搬移操作 → 同一個正規化雜湊 |

### 負向控制（每條都要真的紅）

- 編譯期護欄（上面那條）
- **半搬**：故意讓搬移在中途失敗，測試要抓到「來源沒了但目的也沒有」
- ⚠ 別用「效果為 0」的情境充數——搬一個沒有任何 component 的實體證明不了什麼

## 回報

`wf/inbox/m5-12-cross-zone-complete.md`：驗收表逐條實測、負向控制紅了什麼、
被迫加的新機制對應哪條抽象不夠、現有測試證不了的事。

## 規約

- `cmake --build build --parallel 2`｜不准 fan-out 子 agent｜不要改 `design/`｜不要 push｜繁中、≤ 8 KB
