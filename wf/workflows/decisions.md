# decisions — 決策記錄（為什麼選 A 不選 B）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

留下「為什麼」：git log 找得回改了什麼，找不回**為什麼放棄方案 C**。**本專案的遊戲設計裁定不記這裡**——記在 `design/` 各領域文件（原則在 [principles.md](../../design/principles.md)）與各計畫的 `*-decisions.md`（如 [runtime-injection-decisions](plan/runtime-injection-decisions.md)）；本表只收**工作流／團隊／流程層面**的決定，或當作指向那些裁定的索引。

**何時用**：在兩個以上可行方案裡選一個，且日後可能被問「當初為什麼」。
**何時不用**：只有一條路，或隨時可回頭的小事。決策過程要結構化評估 → knowledge 包的 decide 工作流，結論再落到這裡。

## Done when

- 下表新增一列，且「未選方案與原因」「前提」兩欄非空。

## 記錄（新的在上）

| 日期 | 決定 | 未選方案與原因 | 前提（變了就重看）|
|------|------|---------------|-----------------|
| 2026-09-18 | biglist 清零（契約 [data-files](common/data-files.md)）：**只抽同質記錄表**（矩陣、登記、里程碑、領地、目錄，20 張 → 同目錄 `.json`），**散文式條列一律標 `<!-- wf-nav -->`**（照 kernel 自己 `tidy/gotchas.md`、`dispatch/README.md` 流程的用法） | 全部抽 json：情境走查表、流程、規則論述抽出後 md 剩空殼、違反契約「給人讀的留 md」；按子題拆段：把一段流程拆成半段是假拆 | kernel 的豁免標記只有 `wf-nav`；code-map\* 留給拆檔線收尾 |
| 2026-09-09 | 解除凍結後**先做完整世界規劃、不開始實作**（[roadmap](plan/living-world/roadmap-2026-09.md)）| 直接續 M10 波 2：舊 roadmap 順序要重排，不能當下一件事（[基線](investigation/roadmap-baseline-status.md)）| M10.3a 停在 v23；規劃完成後才排實作順序 |
| 2026-08-27 | **量大的工作全給 codex，Claude 只留裁定、驗收、對使用者**；一波一隻 opus 管家（[TEAM](team-model/TEAM.md)）| 調度長親讀信件與回報：token 最貴、context 撐不住 | codex 月訂閱吃到飽、Claude 按 token 計費 |
| 2026-08-22 | 與 codex 的中途溝通走檔案系統 `.codex-inbox/`，`.ask` 附假設不停、只有 `.blocked` 才停（[CODEX-PROTOCOL](dispatch/CODEX-PROTOCOL.md)）| 只靠回報信：中途卡住沒有出口；Monitor 內嵌：綁 session、改不動 | 規劃者不一定在線上 |
| 2026-08-15／16 | **規劃與實作分離**，規劃者是指揮者、共用同一收件匣、SESSION-LOG 分區塊各寫各的（[CONTACTS](inbox/CONTACTS.md)）| 兩角色各立 inbox：同一個工作資料夾輪流上工，分開沒意義 | 兩個 agent 共寫同一個 repo |
| 2026-09-18 | kernel 升 v0.6 時**沿用單一收件匣與 `<slug>.md` 信件格式**，不採五通道／`PROTOCOL.md`／`inbox_*.sh`；不採 `resources.md`（其「限流按行程樹」與本專案實測相反）| 整套照 v0.6 換：217 封既有信與 CONTACTS 規約全要改 | 規模仍是兩角色＋codex；限流歸屬判斷見 [OPS-NOTES](dispatch/OPS-NOTES.md) |

> 條目多到表格難讀就升級成 `decisions/` 資料夾、一決策一檔（照 [STRUCTURE](../STRUCTURE.md)）。
