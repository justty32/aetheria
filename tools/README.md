# 開發工具

- `check_principle5.py`：內容 enum 檢查的 CLI、allowlist 載入與報告入口；
  `check_principle5_scan.py`：C++ 標頭掃描、註解／字串遮罩與 enum 定位。
- [art/](art/README.md)：確定性美術後處理、入庫閘與獨立 pytest 驗證。

CLI 仍以檔案路徑直接執行，原入口保留既有函式匯出。
