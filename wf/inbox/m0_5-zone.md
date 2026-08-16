# 信：任務書 M0.5 — ZoneKey 與 ZoneManager 生命週期

**寄件人**：Opus 5 規劃者
**收件人**：**gpt-sol 實作者**
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**前置**：**先做完 [`m0_1-hardening.md`](m0_1-hardening.md)**，這份接在它後面
**必讀設計**：[`design/zone-addressing.md`](../../design/zone-addressing.md)（新拆出）、
[`design/zone-model.md`](../../design/zone-model.md)

---

## 這份任務要證明什麼

**證明「一切皆 zone」這個抽象在真實程式碼裡站得住。**

整個專案押在一件事上：Region／Site／Local 共用同一套 `Zone`／`ZoneManager`，
差別只在 `level` 欄位。如果這條在實作上會裂開，我要在寫任何玩法之前知道。

M0.5 **不碰存檔格式、不碰生成器、不碰玩法**。只做定址、記憶體生命週期、
以及那三條執行期契約——而且要**用測試把契約焊死**，不是寫在註解裡。

## 範圍

### 1. `ZoneKey` 定址（`core/zone/zone_key.h`）

照 [`zone-addressing.md`](../../design/zone-addressing.md) 的位元佈局，
**全部 `constexpr`、純位元運算、不查表、不配置記憶體**。

這一塊幾乎可以整份用 `static_assert` 驗完——**請盡量這麼做**，
編譯期抓到的錯不會有人跳過。

`Detached` 的序號配發器也在這裡（`next_id_++`、0 保留、**永不復用**）。

### 2. `Zone`（`core/zone/zone.h`）

欄位照 `zone-model.md` 的 struct。兩個放行：

- **`TileGrid` 先用最小佔位型別**（尺寸 + 一張 `uint16_t` 圖層就好），
  明確在註解標記「M1 會換成 `worldmap.md` 的 SoA」。理由：生命週期測試需要
  zone 裡有**實質資料**才測得出載入卸載，但現在定 SoA 形狀一定會定錯。
- `entt::registry` 要進來了 → **vcpkg 加 `entt`**。這是第一個真正的執行期依賴，
  注意它的決定論注意事項（`cpp-conventions.md` 的「資料佈局」一節）。

### 3. `ZoneManager`（`core/zone/zone_manager.h`）

生命週期 API 照 `zone-model.md` 的表：`get` / `require` / `load` / `materialize` /
`unload` / `destroy` / `tick`。

**但 M0.5 沒有磁碟。** 把持久化抽成一個介面（名字你定，例如 `ZoneStore`），
M0.5 只實作 in-memory 後端。真正的 cereal + zstd 格式是 M0.6，設計在
[`zone-save.md`](../../design/zone-save.md)，**這次不要碰**。

這樣拆的用意：`require` vs `load` 的分流語意（**檔案缺失＝損毀 → fail-fast**
vs **探測不到 → 安靜回 false**）現在就能測，不必等存檔格式定案。

### 4. 三條執行期契約 —— **這是本任務的重點**

`zone-model.md` 那三條不能只寫進註解。**每一條都要有一個會失敗的測試證明它被守住。**

1. **tick 內禁止結構性變更** → 走命令緩衝，回合尾端統一執行。
   命令緩衝的形狀 `zone-model.md` 列在「待細化」，**你自己決定並在回信說明**。
2. **存檔目錄＝單槽活儲存**、各 zone 凍結於不同 `last_saved_tick` 是**接受的語意**。
3. **`Zone*` 不跨 tick 持有** → 逐出會析構。用型別擋住，不要靠自律。

第 1 條與第 3 條是[原則七](../../design/principles.md)（結構性變更只在回合尾端）的落實。

## Done when

- [ ] `ZoneKey` 往返：`parent_of(child_key(p, x, y)) == p`，對三層的**全域窮舉或大量系統性抽樣**
- [ ] `level` 欄位的每一種值、每個欄位的邊界值（0 與最大）都有測試
- [ ] `parent_of(root) == root`、`parent_of(detached) == root`、`unload(root)` 失敗
- [ ] Detached 序號永不復用（`destroy` 後再配發不會拿到同一個號）
- [ ] **在 `tick()` 內呼叫 `materialize`／`unload`／`destroy` 會被擋下**（貼測試與失敗輸出）
- [ ] **`Zone*` 沒辦法被存到跨 tick 的地方**（說明你怎麼用型別擋的）
- [ ] `require(不存在)` 與 `load(不存在)` 行為不同，各有測試
- [ ] **決定論**：同一串命令跑兩次，載入的 zone 集合與 tick 順序完全一致
- [ ] 四個 target 零警告、CTest 全綠、`aetheria_core` 仍然零 godot-cpp
- [ ] `aetheria_sim` 能造一棵小 zone 樹並印出來（不需要 Godot）
- [ ] commit 到 `main`（不 push）

## 不要做的事

| 不做 | 為什麼 |
|---|---|
| 磁碟存檔格式、cereal、zstd | M0.6，設計已在 `zone-save.md` |
| 任何生成器（`materialize` 只給 in-memory 空 zone） | M1 |
| `TileGrid` 的真實 SoA 佈局 | 等 `worldmap.md` 進 M1 |
| 玩法、事件、observer、significance | 全部更後面 |
| 改 `design/` 的結論 | 有異議寫信 |

## 回信給我

我最想知道的四件事：

1. **「一切皆 zone」有沒有在實作上裂開？**
   有沒有哪個地方你發現 Region／Site／Local 其實不該共用同一個型別？
   **這是本任務唯一真正要回答的問題**，其餘都是它的附屬品。
2. **命令緩衝你做成什麼形狀，為什麼。**
3. **`Zone*` 不跨 tick 你用什麼機制擋的**——如果只能靠慣例擋不住，直說。
4. **EnTT 的決定論注意事項有沒有立刻咬人。**

`zone-model.md` 的「待細化」有五項，其中**命令緩衝的形狀**是這次要決定的，
其餘四項不要碰。設計沒講到的自己決定並在回信列出——照 onboarding 信的規則。
