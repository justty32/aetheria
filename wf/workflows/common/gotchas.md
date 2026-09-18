# 共通踩坑（跨工作流）

← [common/README](README.md)｜[INDEX](../../INDEX.md)

不專屬任一工作流的坑記 / 查這裡；工作流專屬的坑記在該工作流自己的 `gotchas.md`（長出來後在下表加一列導流）。

**何時用**：使用者說「記個踩坑」「上次也是這樣」；或動手前要查這個現象以前撞過沒。
**何時不用**：坑專屬某個工作流、而那個工作流已有自己的 `gotchas.md` → 記在那裡；**只撞過一次**的意外 → 不記（門檻是第二次撞到）。

## Done when

- 下表對應的檔多一條坑（粗體標題＋一兩句現象與對策），或查到既有條目並照它處理。

## 哪類坑記哪裡

| 坑的性質 | 記 / 查這裡 |
|---------|------------|
| 共通坑（工具、文件、流程）| **本檔** |
| **會在未來咬人**的專案陷阱：假通過、原則被侵蝕、功能缺口、混淆點 | [KNOWN-TRAPS](../../KNOWN-TRAPS.md) |
| 派工、限流、codex 旗標、合併衝突 | [dispatch/OPS-NOTES](../dispatch/OPS-NOTES.md) |
| 驗收回報被騙過的方式 | [dispatch/OPS-VERIFY](../dispatch/OPS-VERIFY.md) |
| 文件整理與拆檔 | [tidy/gotchas](../tidy/gotchas.md) |

> 條目格式：一條一個坑，**粗體標題＋一兩句現象與對策**。例：`- **存檔已固化**：設定值只是初值，既有存檔保留 runtime 值。`

---

- **`grep --include='*.md'` 在 zsh 要加引號**：不加引號會被 glob 展開成 `no matches found`，整條指令靜默失敗；`wf-lint` 的殘留檢查也靠 grep，指令要在 bash 或加引號跑。
- **worktree 裡跑 `wf-lint`，指向 repo 外的相對連結會假斷**：`wf/workflows/plan/living-world/**` 有十條 `../../../../../../../pas/…` 連結只從 `~/repo/game_dev/aetheria` 根解析得到；在 `.claude/worktrees/<x>/` 跑會多報 BROKEN，不是連結壞了。
