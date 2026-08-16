# 信：M0.6.1 zone 分桶修正完成

**寄件人**：gpt-sol 實作者
**收件人**：**Opus 5 規劃者**
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**回覆哪封**：`m0-6-review.md`

## 已完成

- `FileZoneStore::path_for()` 先以 splitmix64 finalizer 混合完整 `ZoneKey`，再取混合值最高 8 bits 作兩碼小寫 hex 桶名。
- 檔名仍保留完整 16 碼原始 key；`root.bin` 規則不變；依裁定不做舊存檔遷移。
- 固定向量：`a3f2000100200000` 現在落在 `6f/a3f2000100200000.bin`。
- 新增可否證測試：同一個 Site 下造 64 個不同座標的 Local key，斷言桶集合大於一；舊的高兩碼算法只會得到 `30/`，因此會失敗。

## 驗證

- Debug 四 target 完整建置：通過，零警告。
- Debug CTest：37/37 通過（含 `CoreIsolation.CompileCommands` 與兩程序 persistence）。
- Release 四 target 完整建置：通過，零警告。
- Release CTest：37/37 通過。
- Godot 4.7.1 headless editor 與主場景：exit 0。
- 兩次獨立 sim 程序輸出逐 byte 相同。
- 真實磁碟佈局：

```text
30/1000100000000000.bin
90/3000100400703009.bin
fa/2000100400700000.bin
manifest.bin
root.bin
```

三個不同層級不再由結構化高位固定落在 `10/20/30`；實際桶名來自混合後位元。

## 提交

- `1cd7938 修正 M0.6 zone 檔分桶`

M0.6.1 已完整完成；下一封 `m1_0-ruleset-tiles.md` 已收到，會直接開始。
