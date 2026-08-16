# aetheria — AI agent 專案備忘

aetheria = **中世紀奇幻、日式表現風格的三層嵌套地圖回合制策略遊戲；Godot 4 只做顯示／美術／音效／UI，全部玩法邏輯以 C++ GDExtension 實作。** 本檔是**最頂層路由器**：只指向下一層，**durable 細節一律不寫這裡**。

## 先讀哪裡

- **使用者要你動手做某件事** → **[wf/WORKFLOWS.md](wf/WORKFLOWS.md)**：依使用者意圖派發到對應工作流，再讀該工作流入口。
- **想看專案長怎樣** → **[wf/INDEX.md](wf/INDEX.md)**：repo 頂層結構地圖。
- **想看遊戲設計內容** → **[design/README.md](design/README.md)**：設計文件索引。

## 分層思想（本專案的組織原則）

整個 repo 是一棵**分層樹**，每一層**只指向下一層、不存下層的細節**：

```
AGENTS.md（本檔，最頂）→ wf/WORKFLOWS.md / wf/INDEX.md → 各工作流入口 → 工作流內容 → 子工作流…
```

**非侵入式佈局**：repo 頂層只有 `AGENTS.md`（本檔）、`CLAUDE.md` 兩個 agent 入口，工作流 kernel 的其餘部分收在 `wf/`。慣例見 `~/repo/workflows/non-invasive-import.md`。

- **README** = 初入一個資料夾**先讀的入口／導引**；**INDEX** = **描述該資料夾頂層結構**的索引。小資料夾兩者合一，大了才分出獨立 INDEX。
- **durable 知識歸到它所屬的那一層／那個工作流**，絕不往上堆——所以本檔才這麼薄。要某主題的細節，順著上面的樹往下走，不在本檔找。
- **鐵律（always-on，任何工作流任何時候都遵守）**：
  1. 重構/整理必須**不改變原意**（行為不變、改完跑驗證；build/test 指令見 [wf/workflows/testing.md](wf/workflows/testing.md)）。
  2. **未經確認不 push、不開新工作**（commit 到主分支是慣例，push 先確認）。
  3. 各工作流的**具體流程在它自己的入口檔**，不在頂層。
- **[wf/DEV-GUIDE.md](wf/DEV-GUIDE.md) 是被動參考**（結構整理原則 + 四級成長軌跡）——**只在你要重構/整理結構時才取用**，不貫穿日常每個動作。碰原始碼的**程式碼慣例 + 導航 index 維護鏈**在 [wf/workflows/common/conventions.md](wf/workflows/common/conventions.md)。

## 主工作流（活狀態：進度 / 待測 / 信件）

事情告一段落、因應需求結束、或臨時中止時 → 把**還沒完成**的活狀態記到進度；需要**使用者親自做／驗證**的（實機環境、外部工具實跑、需權限/本機環境）→ 記到待使用者。兩者都**只列 open**，完成即移除、不留已完成清單。

- **進度**（我自己的 open in-flight）→ [wf/SESSION-LOG.md](wf/SESSION-LOG.md)
- **待使用者**（等使用者親自做/驗證）→ [wf/WAIT_USER.md](wf/WAIT_USER.md)
- **信件**（agent 之間的訊息交換，像 email；放信處是 `wf/inbox/`）→ 使用方式見 [wf/workflows/inbox/](wf/workflows/inbox/README.md)

## 本地專案規則

- **角色分工（先確認你是哪一個）**：本專案採**規劃與實作分離**。
  **Opus 5 規劃者**寫 `design/` 與任務書、審閱實作，**不寫實作程式碼**；
  **實作 agent** 依 `wf/inbox/` 的任務書寫 `core/`／`bridge/`／`godot/`，**不自行改設計**
  （有異議寫信回報）。兩者共用同一個收件匣，靠信件的收件人欄位辨識。
  分工與流程見 [wf/workflows/inbox/CONTACTS.md](wf/workflows/inbox/CONTACTS.md)。
- **文件鐵律**：每份文件單一檔案、上限 8 KB；超過就依子題拆成多個單檔，**不要拆成資料夾**。
- **文件語言**：繁體中文。
- **設計文件**在 `design/`，索引是 [design/README.md](design/README.md)。
- **架構鐵律**：核心 C++ 邏輯不得依賴 godot-cpp（純 C++ 可獨立編譯與測試）；Godot 端不得持有玩法狀態，只做顯示／美術／音效／UI／輸入轉發。
- **git**：`~/repo/game_dev/` 本身不是 git repo，aetheria 是獨立 git repo（分支 `main`，remote `origin` = `git@github.com:justty32/aetheria.git`）。`third_party/godot-cpp` 是 submodule，clone 後要 `git submodule update --init --recursive`。commit 到 `main` 是慣例；**push 一律先確認**。
- **參考專案**：`~/repo/game_dev/my_godot_assists`（Godot 可複用元件與外部專案分析）、`~/repo/game_dev/my-rpg-frontend`（既有 Godot + GDExtension 專案佈局範例）、`~/repo/moddings/tome4`（下層地圖的 zone／生命週期參考）。

## 專案現況

M0 可編譯骨架已建立：純 C++23 `core/`、GoogleTest、headless CLI、C++23 godot-cpp
GDExtension 與 Godot 4.7 驗證場景均可建置執行；尚無玩法邏輯。建置／測試指令見
[design/build.md](design/build.md) 與 [wf/workflows/testing.md](wf/workflows/testing.md)，程式碼導航見
[wf/workflows/common/conventions.md](wf/workflows/common/conventions.md)。
