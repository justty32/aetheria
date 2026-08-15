# CONTACTS — 通訊錄（已知 agent 的地址）

← [inbox README](README.md)

**地址 = inbox 資料夾位置。** 要寄信給某個 agent，先在這裡查它的 inbox 路徑和它負責什麼。
寄信方式見 [README](README.md)；信裡記得寫**回信地址**。

> 這份是**方便的快取，可能不全**。權威永遠是對方資料夾自己的 `AGENTS.md`／`README.md`。

## 本專案的分工

aetheria 採**規劃與實作分離**的協作模式（使用者於 2026-08-15 訂定）：

| 角色 | 是誰 | 做什麼 | **不做什麼** |
|---|---|---|---|
| **Opus 5 規劃者** | 設計 session | 寫 `design/` 的設計文件、定架構、開任務書、**回頭審閱實作** | **不寫實作程式碼** |
| **實作 agent** | 使用者另外開的 session | 依 `wf/inbox/` 的任務書寫 `core/`／`bridge/`／`godot/` 的程式碼與測試 | 不自行改 `design/`（有異議寫信回報） |

流程是一個循環：

```
規劃者寫設計 + 任務書 → 使用者開實作 agent → 實作 agent 做完寄信回報
   → 規劃者審閱實作、更新設計 → 開下一份任務書 → …
```

## 共用收件匣

**兩個角色共用 aetheria 的同一個收件匣** `~/repo/game_dev/aetheria/wf/inbox/`。

這偏離了 README「一個 inbox 一個 agent」的預設，是刻意的：
兩個角色在**同一個工作資料夾**輪流上工，各自另立地址沒有意義。
辨識靠信件開頭的**寄件人／收件人**欄位，不靠資料夾。

規約不變：**信在頂層 = 未處理，辦完 `mv` 進 `done/`**。
進來先掃頂層，只辦「收件人」是自己的那幾封。

## 通訊錄

| agent（在哪工作） | inbox 地址 | 負責什麼（可以寄什麼給它） |
|---|---|---|
| **Opus 5 規劃者**（`~/repo/game_dev/aetheria`） | `~/repo/game_dev/aetheria/wf/inbox/` | 設計問題、實作完成回報、設計與現實不符的回報、要求開新任務書 |
| **實作 agent**（`~/repo/game_dev/aetheria`） | 同上 | 任務書、設計變更通知、審閱意見 |

## 專案外的可能對象

以下資料夾有自己的 agent 體系，需要時去讀它們的 `AGENTS.md` 確認是否收信：

| 資料夾 | 何時會需要 |
|---|---|
| `~/repo/game_dev/medps` | aetheria 繼承了它的多項決策（見 `design/medps-relation.md`）；那邊 spec 有變動時 |
| `~/repo/game_dev/my_godot_assists` | 需要可複用的 Godot 元件時 |
| `~/repo/moddings/tome4` | L3 下層地圖的參考 |

尚未與這三者建立實際往來；真要寄之前先讀對方的 `AGENTS.md` 確認它有 `inbox/` 且收信。
