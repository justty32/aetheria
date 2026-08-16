# 信：接手 aetheria 實作前，先讀這封

**寄件人**：Opus 5 規劃者（在 `~/repo/game_dev/aetheria` 做設計）
**收件人**：**實作 agent**
**回信地址**：`~/repo/game_dev/aetheria/wf/inbox/`

---

## 你是誰、我是誰

我負責**設計**，你負責**實作**。我不寫實作程式碼，你不自行改設計。
分工與流程見 [CONTACTS.md](../../workflows/inbox/CONTACTS.md)。

具體任務在**另一封信**（目前是 [`m0-bootstrap.md`](m0-bootstrap.md)）。
這封只講「怎麼在這個專案裡工作」。

## 先讀什麼（別跳過，但也別全讀）

`design/` 有 30+ 份文件、單檔上限 8 KB。**不要一次全讀**，照這個順序：

| 順序 | 讀什麼 | 為什麼 |
|---|---|---|
| 1 | [`../../AGENTS.md`](../../../AGENTS.md) | 鐵律 |
| 2 | [`design/README.md`](../../../design/README.md) | 索引 + 導讀主線 |
| 3 | [`design/principles.md`](../../../design/principles.md) | **八條原則。這份最重要** |
| 4 | [`design/outline.md`](../../../design/outline.md) | 全局常數：三層命名、尺度、時間、曆法 |
| 5 | [`design/tech-stack.md`](../../../design/tech-stack.md)、[`design/cpp-conventions.md`](../../../design/cpp-conventions.md) | 三層架構、C++23 約定、vcpkg |
| 6 | 你的任務書點名的那幾份 | 按需 |

其餘的等真的要碰那塊時再讀。文件之間互相連結，順著走就行。

## 三條你最容易違反的鐵律

**1. `core/` 不准碰 godot-cpp。**
不能用 `Vector2`、`String`、`Ref<>`、`UtilityFunctions::print`。
轉換一律在 `bridge/` 做。這條靠 CMake target 擋死——
`aetheria_core` 的 include 路徑裡不得出現任何 godot-cpp。**這條沒有例外。**

**2. 「種類」不准寫成 enum。**
看到 `enum class Terrain { Grass, Desert }` 就是錯的。
地形、河流、道路、建築、單位、事件的種類全部是**資料檔裡的 def + 強型別下標**。
`enum class TerrainId : uint16_t {}` 合法（防混用），但**枚舉子一個都不准列**。
見 [`design/definitions.md`](../../../design/definitions.md)。

**3. 決定論不可妥協。**
同一個 seed + 同一串命令 = 同一個結果，跨平台、跨編譯器。
不用 `random_device`、不讀時鐘、不依賴 `unordered_map` 迭代順序、
遊戲狀態不用浮點數。整個測試策略建立在這條上。
見 [`design/tech-stack.md`](../../../design/tech-stack.md) 的決定論一節。

## 文件規約（你也要遵守）

- **繁體中文**
- **一份文件一個檔案，上限 8 KB**。超過就依子題拆成多個單檔，**不要拆成資料夾**
- 程式碼引用附**路徑:行號**
- 架構圖用 Mermaid／表格／列點，不要 ASCII 框線圖

## 設計錯了怎麼辦

**設計一定會有錯或漏。** 我是照著推演寫的，沒有撞過真實編譯器。

發現問題時：

| 情況 | 怎麼做 |
|---|---|
| 明顯的筆誤、連結壞掉、數值筆滑 | **直接改**，在回報信裡提一句 |
| 設計上行不通（API 形狀不對、效能達不到、庫不支援） | **停下來寫信給我**，附上你撞到的具體證據。**不要自行改設計硬幹** |
| 設計沒講到的細節 | **自己決定並實作**，在回報信裡列出你做的假設 |
| 兩份設計文件互相矛盾 | 寫信問我，別自己挑一邊 |

第二類特別重要：你撞到的現實比我的推演可靠。**帶著證據回報，比默默繞過去有價值得多。**

## 回報方式

做完一份任務書之後：

1. 把任務書 `mv` 進 `wf/inbox/done/`
2. 寫一封新信丟回 `wf/inbox/`，收件人寫「Opus 5 規劃者」，內容包含：
   - **實際做了什麼**（檔案清單 + 一句話）
   - **`Done when:` 逐條核對結果**（做到了／沒做到／做法不同）
   - **你做的假設**（設計沒講到而你自己決定的）
   - **設計與現實不符之處**（附證據，這是最有價值的部分）
   - **測試結果原文**（別轉述，貼輸出）
3. 更新 [`../SESSION-LOG.md`](../../SESSION-LOG.md)（只列 open 項）與
   [`../WAIT_USER.md`](../../WAIT_USER.md)（需要使用者親自驗的）

**沒完整完成就別回信**，卡住的話在 SESSION-LOG 記下來、去問使用者。

## 幾個會省你時間的提醒

- **`~/repo/game_dev/medps` 是同一構想的前一輪**，有可跑的 `gcore/`（EnTT + cereal + zone）、
  Godot GDExtension 設定與 26 項測試。aetheria 的多項決策直接繼承自它——
  遇到「這要怎麼寫」先去那邊看有沒有現成的。見 [`design/medps-relation.md`](../../../design/medps-relation.md)。
- **`~/repo/game_dev/my-rpg-frontend`** 有可直接抄的 Godot + GDExtension 專案佈局
  （`gdext/CMakeLists.txt`、`bin/*.gdextension`）。
- **`~/repo/game_dev/my_godot_assists`** 有多個現成的 Godot 元件（世界地圖分層、相機、
  選取高亮、小地圖），顯示層優先取用，不要重寫。
- godot-cpp 用 C++23 重編**是已知可行的**——使用者先前用 C++20 重編過。

## 不需回信

這封是說明，不是任務。看完辦 [`m0-bootstrap.md`](m0-bootstrap.md)，然後把**這封**也一起
`mv` 進 `done/`。
