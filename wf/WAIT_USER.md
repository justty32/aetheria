# WAIT_USER — 等待使用者的事

← [AGENTS.md](../AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證**才能繼續的事——例如：實機/實環境測試、外部服務登入、環境變數設定、權限操作、需要帳號的下載、**催/開/fork 另一個 agent 處理急件**（見 [inbox](workflows/inbox/README.md)：寄了信但很急、對方可能沒開）。Claude 能做結構性驗證＋打包到極限；跨不過去的那一關記這裡等使用者。

**只列還沒做的**——做完即移除（不留已完成清單，歷史看 git log）。

> **膨脹就拆**：待使用者項堆多了，就開 **`wait_todo/`** 資料夾按類別拆檔，本檔退回只留導航到各分類檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

## 待使用者項

- **`workflows/inbox/CONTACTS.md` 目前是空的**（無已知協作 agent）。
  日後真有協作對象再補，沒有編造假地址。

> 已裁定並移除的項目（2026-08-15）：
> medps 關係 → **取向 ②：獨立但逐條繼承已拍板決策**，繼承核對清單在
> [design/medps-relation.md](../design/medps-relation.md)；
> 時區 → **台北時區**，推斷值確認無誤，兩個檔不必改。
