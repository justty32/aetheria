# 任務書 M10.0a — 把原則五做成一條會紅的 ctest

**寄件人**：規劃者（調度 session）
**收件人**：gpt-sol 實作者
**必讀**：[`CODEX-PROTOCOL.md`](../CODEX-PROTOCOL.md)、[計畫](../workflows/plan/runtime-injection-waves.md) M10.0a 節
**工作分支**：`m10-0a`（worktree `../aetheria-wt-m10-0a`）

## 背景

原則五「種類是資料，不是 enum」只是文件裡的一句話，417 個測試全綠也擋不住新違規
（M8.3 的 `PersistentBuildingType` 就是規劃者批准下溜進來的）。你的任務：讓它變成測試。

## 要做的

1. 一支掃描腳本（建議 `tools/check_principle5.py`，python3 標準庫、無新依賴）：
   掃 `core/` 全部標頭，找出**帶枚舉子的 `enum class`**（空的 `enum class X : uint16_t {}`
   合法，不報）。注意排除註解與字串裡的假匹配。
2. 白名單檔（建議 `tests/principle5_allowlist.txt`，格式自定但要有註解欄）分兩段：
   - **機制 enum（永久合法）**：狀態與機制類，如 `BuildingState`、`LodLevel`、`TurnStage`、
     `DoorState`、`CohortFacing`……實掃後逐一登錄，每條寫一句為什麼是機制不是內容。
   - **待清償（已知違規，只准遞減）**：`CohortRole`、`PersistentBuildingType`、
     `FactionGoal`、`FactionActionKind`、`DungeonArchetype`、`TrapKind`、
     `EmergentQuestKind`、`LocalRoomKind`。若實掃發現第 9 個內容種類 enum，
     加進這段並在回報裡標紅。
3. 判定規則：掃到的 enum 不在白名單 → **fail**（訊息要指出檔案:行號與該去哪一段）；
   白名單「待清償」段裡的 enum 已從程式碼消失 → **fail**（訊息：從白名單刪掉它）——
   這樣清償進度永遠與現實同步。
4. 掛進 ctest（`add_test`），跑進既有測試套件。

## 驗收（就這 4 條，不要多跑）

| # | 標準 |
|---|---|
| 1 | 現況全綠：`ctest -R principle5` 通過，附輸出 |
| 2 | 負向控制：臨時在 `core/` 加一個假內容 enum（如 `enum class FakeTerrainKind { A, B };`）→ 測試紅、訊息含檔案:行號；還原後綠。附兩次輸出 |
| 3 | 負向控制：從白名單「待清償」段臨時刪一列 → 紅；還原後綠 |
| 4 | 全套 `ctest` 綠（`--parallel 2`），總數不減 |

## 模組與依賴（原則九，回報必填）

新程式碼放哪、依賴誰。預期：`tools/check_principle5.py` 零依賴、
白名單在 `tests/`、CMake 只加 tests 區段一條 `add_test`。

## 不要做的事

| 不要 | 理由 |
|---|---|
| 動 `core/`、`bridge/`、`godot/` 任何一行（負向控制的臨時改動除外，必還原） | 領地只有 `tests/`＋`tools/`＋CMake tests 區段 |
| 動既有測試檔 | M10.0b 正在並行改雜湊測試 |
| 動 `kSaveFormatVersion`、`core/serialize/` | M10.0c 獨佔 |
| 修白名單裡的違規本身 | 那是波 3 的事，這輪只登記 |
| build 用超過 `-j2`、ctest 超過 `--parallel 2` | 同機另有兩路在編譯 |
| push｜fan-out 子 agent｜改 `design/` | 一律禁止 |

## 回報

`wf/inbox/m10-0a-principle5-test-complete.md`（≤8KB，繁中）：
第一行標 `DONE`/`BLOCKED`/`NEEDS-USER`/`FAILED`；兩個負向控制的前後輸出、
機制段每條的一句理由、實掃是否發現第 9 個違規、模組與依賴節。
中途疑問寫 `.codex-inbox/m10-0a.ask`（**寫下假設繼續做**）；卡超過 10 分鐘必發。

**最重要**：如果掃描規則有你判不動的灰色地帶（像「這個 enum 到底是機制還是內容」），
把判不動的逐條列出來讓我裁定，不要硬塞進某一段糊過去。
