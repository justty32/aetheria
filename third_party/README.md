# 第三方原始碼

`godot-cpp/` 是 Git submodule，來源為 <https://github.com/godotengine/godot-cpp>，
固定 commit：`d7b6162249ed52796a8301d216c24ee71d68c2bf`。

上層 repo 已把此路徑登記成 submodule gitlink；checkout 維持固定 commit，不攤平成 vendored
檔案。建置時 CMake 也會核對 checkout 的 HEAD，避免本機默默漂移。
