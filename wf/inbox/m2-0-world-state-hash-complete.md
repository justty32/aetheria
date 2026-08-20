# 信：M2.0 世界級正規化狀態雜湊完成

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**改前基準**：`6596c58`

## 落地內容

- 新增 `aetheria_sim verify world-hash <save_dir>`；實作只存在 `sim/world_hash.*`，不進
  `aetheria_core`，不常駐、不寫存檔、不被 tick／populate／load 呼叫。
- 工具直接遞迴列舉 slot 下所有 zone `.bin`（排除 `manifest.bin`），由檔名還原 `ZoneKey`，
  拒絕非 key 推導出的 canonical 路徑；每檔沿用 `FileZoneStore::load` 與既有
  `normalized_state_hash`。
- per-zone 結果先依 `ZoneKey` 排序，再合併為世界雜湊。新增 CMake 守門測試，掃描
  `core/`、`bridge/`、`godot/`，一旦玩法來源引用 `world_state_hash` 或工具 header 就失敗。
- per-zone 雜湊只做必要泛化：root／Site／Local 可合法沒有 `TurnClock`，多於一個仍 fail-fast；
  有無 clock 本身與 clock 值都進 hash。未改存檔格式。

## 三條證明

測試世界包含 root 加 3 個 Region zone。驗證前的 `ZoneManager` 只載入 root，三個 Region
逐一斷言未載入；世界雜湊仍讀到 `zone_count=4`，因此證明資料來自磁碟列舉。

### (a) 跨建構歷史等價

正向歷史依 `Region 7 → 2 → 13` 建檔，且每區先建 StableId 10 再建 20；反向歷史依
`13 → 2 → 7` 建檔，每區先建 20 再建 10。邏輯狀態相同：

```text
forward world_hash = 14574165762117914134
reverse world_hash = 14574165762117914134
```

### (b) 存檔 bytes 確實不同

下面只合併三個 Region 的實際 zstd `.bin` bytes；root 與 manifest 相同，刻意不拿來稀釋證據：

```text
forward bytes_hash = 8653445777865804353,  bytes = 1365
reverse bytes_hash = 18016500724071268962, bytes = 1369
```

byte hash 與長度都不同，(a) 不是兩邊其實同 bytes 的空證明。

### (c) 負向控制

只把 StableId 10 的 `MovementPoints.current` 加一，世界雜湊改變；只改 `Zone::lod`、
`Zone::pinned`、`SiteState::lod` 後重新存檔，雜湊與 bytes 都不變：

```text
original_hash  = 14574165762117914134
runtime_hash   = 14574165762117914134
component_hash = 13715543698692156533
```

## 三個回答

1. **bytes 真的不同。** 是；兩側分別 1365／1369 bytes，byte hash 也不同，如上。沒有用
   「世界雜湊相同」掩蓋相同 bytes。
2. **合併使用同一條 64-bit FNV-1a。** 排序後先餵 zone 數量，再對每項依序餵
   `ZoneKey` 與 per-zone hash，所有整數固定按 little-endian byte 順序餵入。沒有用 XOR；key
   與內容綁在同一條有順序的輸入流，兩個 zone 交換內容會改變結果。
3. **磁碟成本為 4 zones、0.390019 ms。** 這是 Debug `aetheria_sim verify world-hash` 的單次
   實測，計時範圍包含遞迴列舉、zstd 解壓、decode、per-zone 正規化與合併，不含 Ruleset
   載入。現階段遠低於測試瓶頸；M4 zone 規模放大後可沿用 CLI 重測。

## 健壯性與完整驗證

- 缺目錄、空目錄、損毀 zone、zone 版本不符各有負向測試；錯誤含目錄或精確 `.bin` 路徑，
  不略過壞檔。manifest 開啟失敗會明確包上 `manifest.bin`。
- 四 target 以 `cmake --build build --parallel 2` 完整建置，零警告。
- CTest **133/133**，包含 `CoreIsolation.CompileCommands`、`SimWorldHash.Command` 與
  `VerificationBoundary.WorldHash`。
- `aetheria_sim --tick 62208000`、Godot headless editor、Godot headless 主場景皆 exit 0。
- `git diff --check` 通過；未做 Site 展開／收回，未動生成、氣候、biome、地物、勢力或存檔格式；
  未 push。

請完整審閱後再回信。
