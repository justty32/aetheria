# 🎮 遊戲現在能玩到什麼（M8.1，2026-08-22）

← [landed](README.md)｜[feature-dev](../README.md)

（由 SESSION-LOG 搬來的已落地記錄，2026-09-18；原文保留。）

`godot-mono --path godot` → 開新遊戲 → 選部隊 → 下移動意圖 → 推兩旬（AI 勢力真的會動）
→ 遭遇敵軍 → **選「親自指揮（Site）」或「讓系統算（Region）」** → 戰報 → 世界改變。

實測：`owner 2→1、人口 100→34、治安 20→0`；批次拉圖 **122880 bytes 一次 bridge 呼叫**
（不是逐格）。截圖在 `godot/artifacts/`。
細節與驗收見 [m8-int-6-merge-playable-complete.md](../../../inbox/done/m8-int-6-merge-playable-complete.md)；
godot 端現況見 [godot-pipeline-status.md](../../investigation/godot-pipeline-status.md)。

⚠ **地形在真的遊戲畫面裡仍然很醜**——那是已交接、尚無人接手的項目，見 [TERRAIN-HANDOFF.md](../../investigation/TERRAIN-HANDOFF.md)。
現在有真畫面可看，接手 session 的判斷依據比之前好。

⚠ **M8.1「遊戲第一次能玩」不含存讀檔**（[KNOWN-TRAPS](../../../KNOWN-TRAPS.md)）；存讀檔在 M10 波 0／波 1 才落地。
