# TEAM — 團隊角色與執行節奏（2026-08-27 使用者裁定）

← [AGENTS.md](../AGENTS.md)｜[OPS-NOTES](OPS-NOTES.md)｜[CODEX-PROTOCOL](CODEX-PROTOCOL.md)

> **成本前提**：Claude（調度長＋opus/sonnet agent）按 token 計費，**稀缺**；
> codex（gpt-sol／terra，$200 月訂閱）**吃到飽**。
> 原則：**量大的工作全部給 codex；Claude 只留判斷、驗收、對使用者。**

## 角色

| 角色 | 是誰 | 做什麼 | 不做什麼 |
|---|---|---|---|
| **調度長** | 主 session（Fable/Opus） | 對使用者；**裁定**；波終驗（親手重跑至少一條負向控制）；併 main；維護 wf/ 與記憶 | 不寫實作、不起草長文、不逐則盯 `.ask` |
| **波管家** | opus subagent，**一波一隻** | 拿波 brief 執行整波：叫 codex 起草→送調度長批→派工→監看→按「裁定包絡」回覆例行 `.ask`→**預驗**（重跑 ctest＋負向控制）→交一份波末包裹 | 不自己寫碼、不自創裁定（包絡外一律上報）、不對使用者 |
| **codex sol 工作線** | `codex exec`（效率 high） | **實作、整合、起草任務書、調查報告**——一切產文字/程式碼的量大活 | 不 push、不改 design/、不碰他線領地 |
| **codex terra 子線** | sol 自己開的 subagent | 分工（互斥線內拆檔）、**交叉審查** sol 的 diff | 純自審 fan-out 重工照舊禁止 |

## 通訊（沿用既有兩通道，不引入 skyrim 的 mail/topics——規模用不到）

- **任務書／回報信**：`wf/inbox/`（辦完歸 `done/`）。起草線的草稿也走這裡
  （`<round>-draft.md`，調度長批完改名定稿）。
- **即時通道**：`.codex-inbox/`（[CODEX-PROTOCOL](CODEX-PROTOCOL.md)：`.ask` 附假設不停、
  `.blocked` 才停、`.reply` 裁定）。
- **裁定包絡**：波 brief 裡明列「管家可自答的範圍」（計畫檔已裁定的、KNOWN-TRAPS 已記的、
  [OPS-VERIFY](OPS-VERIFY.md) 的通則）；包絡外的 `.ask` 一律升級調度長。

## 執行節奏（以波為單位）

```
調度長：發波 brief（輪次表＋裁定包絡＋驗收底線）
  → 波管家：codex 起草信 → 調度長一次批整波的裁定點
  → 波管家：逐輪派 codex（實作→terra 交叉審→回報）→ 預驗
  → 調度長：波終驗（親手負向控制抽查）→ 併 main → 歸檔 → 下一波
```

調度長每波醒 **~3 次**（brief／批裁定／終驗），其餘喚醒都由背景通知與管家吸收。

## 驗收的不可讓渡底線

- 文字回報永遠不足採信（[OPS-VERIFY](OPS-VERIFY.md)）：**負向控制必須有 Claude 的手重跑過**
  ——管家預驗算數，但調度長併 main 前仍抽查至少一條。
- terra 交叉審是**多一雙眼**，不是驗收的替代。

## Context／compact 管理

- **調度長**：自己盯預算，逼近就請使用者 `/compact`。
- **opus/sonnet agent**：**用生命週期管，不用 compact 管**——一波一隻管家、一題一隻調查兵，
  做完就死；題目大到可能撐爆一個窗，就先拆題再派，**不讓任何 agent 跑到窗邊緣**。
  管家波中若接近上限，把狀態寫進波末包裹格式提前交棒，調度長再開一隻續波。
- **codex**：自帶 auto-compact，不用管。

## 派工細節不重抄

指令、旗標坑、限流、並行 worktree、合併括號坑 → [OPS-NOTES](OPS-NOTES.md)；
M10 的領地與版本政策 → [plan/runtime-injection-dispatch.md](workflows/plan/runtime-injection-dispatch.md)。
