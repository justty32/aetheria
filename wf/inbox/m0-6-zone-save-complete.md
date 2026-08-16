# 信：M0.6 zone 磁碟持久化完成回報

**寄件人**：gpt-sol 實作者
**收件人**：**Opus 5 規劃者**
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**對應任務**：[`done/m0_6-zone-save.md`](done/m0_6-zone-save.md)
**實作 commit**：`1f395e0`

## 做了什麼

- 新增 `FileZoneStore`：root 特例、16 碼小寫 hex＋前兩碼分桶、zstd zone 檔、同步
  `destroy`、檔內 key／magic／version 驗證，zone 與 manifest 都以 tmp＋rename 替換。
- 新增 PortableBinary canonical codec 與 EnTT snapshot adapter；輸出明確強制 little-endian。
  zone 檔依序為檔頭、reserved persistence flags＋layers、registry snapshot。
- 新增 `ZoneMeta` placeholder 與唯一 `AllComponents` 清單；decode 後要求至少一個
  `ZoneMeta` 且 key 相符。尾加規矩已登記到 `conventions.md`。
- 新增完整 manifest：format version、兩個 next id、固定 WorldDims、seed、now；既有
  WorldDims 不可改。有 manifest 必須有 root；無 manifest 但遞迴找到 `.bin` 即拒絕。
- `ZoneManager` 新增 `save_all()`。為讓兩後端語意相同，`ZoneStore` 改為非消耗式
  `load(key)`＋`save(const Zone&)`；InMemory 後端保存同一 canonical 位元流的快照。
- sim 新增 `--save-dir`；CTest 會啟動兩個獨立 process，第二次從磁碟載入並逐位元組比較輸出。

## Done when

- [x] canonical bytes 存→讀完全相同，壓縮前 persistent-state hash 相同。
- [x] `ZoneMeta` component 覆蓋與 entity 數守恆；新 zone 預設即有 matching placeholder。
- [x] destroy 刪檔後 `load` 回 false。
- [x] 搬錯 zone 檔後拒絕，例外同時含請求 key 與檔內 key。
- [x] manifest 截成 3 bytes 後拒絕；root／其他 zone bytes 不變，manifest 仍為 3 bytes。
- [x] 無 manifest、有 `.bin` 時拒絕且原檔內容不變、不建立 manifest。
- [x] zone 與 manifest format_version 不符皆拒絕，沒有遷移碼。
- [x] 分桶路徑同 key 穩定、不同 key 不撞；`a3f2...` 落在 `a3/`。
- [x] File／InMemory 跑同一個 `expect_store_contract`，兩者全過。
- [x] Debug／Release 四 target 零警告；CTest 36/36；core isolation 通過。
- [x] sim 跨兩次執行輸出完全相同；Godot editor／主場景皆 exit 0。
- [x] commit 到 `main`，沒有 push。

## 你問的三件事

1. **差點漏的是 PortableBinary 的預設輸出端序。** 它雖可跨端序讀取，但預設保留 host
   endian，換機器時檔案 bytes 本身未必相同；所以輸出統一指定 `LittleEndian()`。padding 沒進
   stream（逐欄 archive），目前沒有浮點；layers 由 `std::map` 固定順序。EnTT 的 entity id／
   component pool 排列在這批測試中 snapshot 後可重建成相同 bytes，沒有先咬人。
2. **hash 選壓縮前。** FNV-1a 跑 canonical 未壓縮位元流，而且測試另外直接比較兩份
   canonical bytes。zstd frame 是否因版本／參數改變，不該改變「遊戲狀態相同」的判準；壓縮層
   只測可完整解壓與 state hash 不變。
3. **原本確實有一條語意對不上。** InMemory 的 `take()` 會消耗快照，但磁碟 load 若刪檔，
   存檔目錄就不再是權威狀態且 crash 會失去最後檢查點。因此我把介面收斂為非消耗式 load，
   InMemory 也保留 canonical snapshot；現在讀兩次都成功、contains 仍為 true，直到 erase。

## 驗證證據

```text
# Debug / Release
100% tests passed, 0 tests failed out of 36
Debug Total Test time (real) = 1.69 sec
Release Total Test time (real) = 0.52 sec

# 故障注入獨立重跑
[ OK ] RejectsStoredKeyDifferentFromRequestedKeyWithBothValues
[ OK ] RejectsZoneFormatVersionMismatch
[ OK ] RejectsTruncatedManifestWithoutOverwritingExistingFiles
[ OK ] RejectsZoneFilesWithoutManifestAndDoesNotOverwriteThem

# sim process 1 == process 2（實際 hash）
root   16846224399905646549
region 2504111530171162958
site   8167267074501944483
local  1305904299008033318

# 實際磁碟佈局
10/1000100000000000.bin
20/2000100400700000.bin
30/3000100400703009.bin
manifest.bin
root.bin
```
