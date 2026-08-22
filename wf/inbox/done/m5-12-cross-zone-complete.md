# M5.12 完成回報 — 跨 zone 讀與實體搬移

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回報任務**：[`done/m5-12-cross-zone.md`](done/m5-12-cross-zone.md)

## 落地內容

- 新增 runtime-only `CrossZoneRuntime`：`peek_tile`／`peek_edge` 回不可變值快照，未載入、
  非 Local、缺 ground layer、越界均回 `nullopt`，介面為 `noexcept`。
- 新增 `EntityRef{zone, uid}`、易失層 `LocalPosition`；跨 zone 解引用只走 `uid_index`。
- `migrate_entity` 先 staging 目的 entity、複製非空 component、寫新座標與目的 uid index，
  全部成功才刪來源；失敗銷毀 staging，來源不動。目的搬入成功時 `touch_count + 1`。
- `ZoneManager` 在 load／adopt 時重建並驗證 `uid_index`；正規化雜湊納入 LocalPosition。
- `core/worldgen` 拆成獨立 object target，只有 repo 根 include path；runtime API include 根只給
  `aetheria_core` consumer。未修改任何 worldgen 原始碼。

## 驗收實測

| 判準 | 結果與見證 |
|---|---|
| 生成期不能跨 | 直接以生成 target include 根編譯，exit 1：`fatal error: aetheria/runtime/cross_zone.h: No such file or directory`；CTest wrapper 綠 |
| 未載入退化 | 未載入目的 `peek_tile`／`peek_edge` 均 `nullopt`，`EXPECT_NO_THROW` 綠；越界亦 `nullopt` |
| 舊 handle 失效 | uid 77 搬移後來源 `reg.valid(old_handle) == false`，來源 ref 解不到；目的 ref 解到新 handle |
| 失敗回 false、來源不動 | 無可載入目的時回 `false`；uid 88 的 handle、`MovementPoints{7,13}`、座標 `(3,4)`、來源索引均原值；tick 中缺目的也回 false、不觸發結構載入 |
| uid_index 同步 | 成功後來源不含 uid 77，目的 `uid_index[77]` 指向新 handle；中途 uid 衝突 rollback 後兩邊仍各指原實體 |
| component／位置／touch | 搬入後保有 `StableId{77}`、`MovementPoints{7,13}`，LocalPosition 改為 `(60,61)`，目的 touch `0 → 1` |
| 決定性 | 同一串 A→B→A 搬移兩次，兩 zone 正規化雜湊皆相同：`8766003291610299300,16658015200527722862` |

## 負向控制（直接執行真的紅）

1. **編譯期護欄**：直接編譯 `tests/compile_fail/worldgen_cross_zone_include.cpp`，exit 1，
   精確錯誤為 `aetheria/runtime/cross_zone.h: No such file or directory`。
2. **半搬**：刻意錯誤版本先刪來源、才發現目的 uid 衝突並清掉 staging；直接跑負控 binary
   exit 1，輸出 `source_valid=0 destination_migrated=0`，兩個 assertion 分別報：
   `Actual: false Expected: true`（來源消失）與 `destination_migrated Which is: 0 ... 1`
   （目的也沒有帶 `MovementPoints` 的搬入實體）。不是空 entity／零效果情境。

CTest wrapper 只在上述非零退出且錯因／數字吻合時才綠；若負控不紅或紅錯原因會失敗。

## 完整驗證

- `cmake --build build --parallel 2`：綠。
- `ctest --test-dir build --output-on-failure`：**236/236 綠**（含兩條預期失敗 wrapper）。
- `./build/aetheria_sim --tick 62208000`：exit 0，固定 zone tree/hash 輸出正常。
- Godot 4.7.1 headless editor 與主場景：皆 exit 0。

## 抽象不足／尚不能證明

- **現有 `ZoneManager::load` 不等於 Local rematerialize**：codec 刻意不存程序 tiles；manager
  也尚無「載入失敗後呼叫 Local 生成／重展開」callback。故本輪只嘗試載入已準備好的完整 zone，
  缺席、I/O 例外或只有空 LocalPayload 時回 `false`；沒有用 `materialize()` 假造空地形。
  待 M5.11 保留的地形接口落地後，應把 callback 接到這個單一入口。
- 尚無格→實體空間索引；本輪以 LocalPosition 建立唯一 commit 點，未來索引維護掛在同一點。
- migratable component 是明確白名單；遇到未登記 component 會原子失敗、不會靜默漏搬。
  未來新增可跨 Local 的 component 必須同步加入白名單。
- 測試可證 uid 衝突發生在 staging 後仍能 rollback；無法可重現地注入 allocator failure。
- 依任務排除，未做 FOV、潛行、光照、尋路退化、事件快速路徑、ReturnTrail 或 L3 串流消費 touch。

`design/`、`core/worldgen/`、`data/biomes.toml`、`sim/` 均未修改；未 push。
