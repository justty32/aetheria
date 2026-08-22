# CODEX-PROTOCOL — 規劃者與 codex 的即時通道

← [SESSION-LOG.md](SESSION-LOG.md)｜[OPS-NOTES.md](OPS-NOTES.md)

> **為什麼要有這個**：回報只在 codex **做完並 commit** 之後才看得到。
> 中途卡住、發現任務書寫錯、需要裁定——這些**在舊做法裡完全沒有出口**，
> codex 只能自己猜著做完，我才在回報裡看到「我假設了 X」。
> 而且 Monitor 綁在 session 上，session 一斷就瞎了。
>
> 這條通道走**檔案系統**：不進版控、不需要 commit、不綁 session。

## 目錄

```
.codex-inbox/          ← gitignore，兩邊都直接讀寫
```

## codex 寫什麼

| 檔名 | 什麼時候寫 | 寫完之後 |
|---|---|---|
| `<round>.progress` | 完成一個里程碑時（選填） | 繼續做 |
| `<round>.ask` | 有疑問，但**猜得下去** | ⚠ **寫下你的假設然後繼續做**，不要等 |
| `<round>.blocked` | 真的做不下去（任務書自相矛盾、前置不存在） | **停下來** |
| `<round>.done` | 全部完成並 commit 之後 | 結束 |

⚠ **`.ask` 不會讓你停下來。** 規劃者不一定在線上，等於死鎖。
規則是：**寫下問題 + 你採用的假設，然後照那個假設做完**。
規劃者看到後若裁定不同，下一輪修正——這比停在那裡便宜得多。

**只有 `.blocked` 才停**，而且要寫清楚「缺什麼才能繼續」。

## 格式

第一行是給人看的一句話，後面隨意：

```
標題：一句話說完
（空行）
細節、數字、你採用的假設、需要什麼
```

## 規劃者寫什麼

| 檔名 | 意思 |
|---|---|
| `<round>.reply` | 對 `.ask`／`.blocked` 的裁定 |

被 `.blocked` 擋住的回合，用 `codex exec resume --last`（要在同一個 cwd）接回去。

## 監看

```bash
scripts/codex-watch.sh
```

寫成**檔案**不是內嵌在 Monitor 裡——內嵌就改不動（OPS-NOTES 記過這條）。
它同時看三件事：收件匣新檔、各 worktree 分支上新出現的 `*-complete.md`、
以及**我的 codex 是不是全部消失了**（異常結束的唯一訊號）。

⚠ 判斷「我的 codex」一律讀 `/proc/<pid>/cmdline` 的 `-C` 參數比對 session id，
不要用名稱、行程樹或 `cwd`（三種都試過都錯，見 OPS-NOTES）。
