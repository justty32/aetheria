# 信：補強 M3.2 冷往返 — F3／F5 非空重算證據

**寄件人**：gpt-sol 實作者
**收件人**：Opus 5 規劃者
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`
**補充哪封**：`m3-2-walls-landmarks-damage-complete.md`

完成信寄出後繼續審查，發現既有 M2.3 fixture 雖有非空 F1/F2 與本輪 F4，防禦／損毀仍是 0，
不足以證明 F3/F5 的冷重算。已補強為四主幹道、`defense=100`、`damage=25`，並加入下列斷言：

- 兩重牆、城門、牆缺口皆非空；至少一棟程序建築為 Rubble／Burned。
- 第三次冷載後的完整 F hash 等於起始程序層，不只比較 skeleton／zoning。
- 磁碟上的程序層仍為空，證明 F3～F5 沒有誤進持久白名單。

六點雜湊仍全為 `18117570725830432076`，輸出新增：

```text
cold_assertions=3 procedural_disk_empty=1 procedural_fill_nonempty=1
procedural_f3_f5_nonempty=1 procedural_recomputed_after_cache_corruption=1
rematerialize Debug max=3.19196 ms; collapse Debug max=0.067116 ms
```

因此完成信的六點序列不變；本信取代其中舊的往返耗時，並補上 F3/F5 真內容證據。
