# 第三方原始碼

`godot-cpp/` 是 Git submodule，來源為 <https://github.com/godotengine/godot-cpp>，
固定 commit：`d7b6162249ed52796a8301d216c24ee71d68c2bf`。

工作樹裡目前是同 commit 的 detached checkout，但尚未登記進上層 repo index；提交 M0 前應把
此路徑登記成 submodule gitlink，不要把它攤平成 vendored 檔案。建置時 CMake 會核對 checkout
的 HEAD，避免本機默默漂移。
