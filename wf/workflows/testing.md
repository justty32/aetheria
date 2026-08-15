# testing — 跑測試（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

## 指令

- **快速驗證（Claude 自己跑、鐵律要求的那套）**：尚未建立（規劃階段，尚無 CMake / Godot 專案骨架，見 [design/](../../design/README.md)）。
- **完整驗證**：尚未建立，同上。

## 測試分類

尚無測試分類——目前無程式碼、無 build/test 骨架。日後建立 C++ 核心（預期用 CTest 或同等框架，純 C++ 不依賴 godot-cpp 部分應可獨立跑單元測試）與 Godot 端整合驗證時，回來補上分類方式（例如：純 C++ 單元測試 vs. 需要 Godot runtime 的整合/場景測試）。跑不了的環境依賴驗證 → 記 [WAIT_USER](../WAIT_USER.md)。
