# TEAM — 團隊角色與執行節奏（2026-08-27 使用者裁定）

← [AGENTS.md](../AGENTS.md)｜[OPS-NOTES](OPS-NOTES.md)｜[CODEX-PROTOCOL](CODEX-PROTOCOL.md)

> **成本前提**：Claude（調度長＋opus/sonnet agent）按 token 計費，**稀缺**；
> codex（gpt-sol／terra，$200 月訂閱）**吃到飽**。
> 原則：**量大的工作全部給 codex；Claude 只留判斷、驗收、對使用者。**

## 角色

| 角色 | 是誰 | 做什麼 | 不做什麼 |
|---|---|---|---|
| **調度長** | 主 session（Fable，token 最貴） | 對使用者；**裁定與批准**——只讀管家上呈的**決策摘要**，批一句話 | **不親讀任何信件／回報**、不寫實作、不起草、不盯 `.ask`、不跑驗證指令 |
| **波管家** | opus subagent，**一波一隻** | **inbox 與 codex 的唯一第一讀者**。執行整波：叫 codex 起草→按包絡定稿→派工→監看→回例行 `.ask`→**親手驗收**（重跑 ctest＋每條負向控制——Claude 之手的要求由管家滿足）→獲批後**併 main、歸檔、記 SESSION-LOG** | 不自創裁定（包絡外上報）、不對使用者、上呈摘要 ≤30 行不貼原文 |
| **sonnet 雜務手** | 管家開的 sonnet subagent | 管家的雜事：掃檔核對行號、跑測試收輸出、grep 覆核、盯長時間監看——**省管家自己的窗** | 不做判斷性工作（驗收裁量、定稿、裁定仍是管家的）；一題一隻用完即死 |
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
  → 波管家：整波自轉（起草→派工→.ask→逐輪親手驗收）
     包絡外 → ESCALATION 摘要上呈 → 調度長一句裁定
  → 波管家：波末交「決策摘要」（≤30 行：結論／證據摘要／格式變更／pending）
  → 調度長：批准一句話 → 波管家併 main、歸檔、記 SESSION-LOG → 下一波
```

調度長每波理想上只醒 **2 次**（brief＋批准；有 ESCALATION 才加）。
**所有信件、回報、`.ask` 的原文只進管家的 context，永不進調度長的。**

## 驗收的不可讓渡底線

- 文字回報永遠不足採信（[OPS-VERIFY](OPS-VERIFY.md)）：**負向控制必須有 Claude 的手重跑過**
  ——這隻手是**管家的**；管家上呈的摘要必須寫明「哪幾條是我親手注入看紅的」。
- 調度長保留抽查權（摘要可疑時），但抽查是例外不是流程。
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
