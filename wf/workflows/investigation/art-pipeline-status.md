# art-pipeline — 美術管線做到哪了

← [investigation](README.md)｜2026-08-27

## 問題

使用者原話：「美術管線做到哪了？從「生成素材」到「畫面上看到」的整條路通了幾成？」

拆成的可判定提問：（1）`tools/art/` 有哪些工具、輸入輸出格式、測試幾條？
（2）[art-pipeline.md](../../../design/presentation/art-pipeline.md) 的四道閘各在什麼狀態、哪些卡使用者？
（3）素材怎麼進 godot、godot 端有沒有吃圖的程式碼、佔位素材誰生成？
（4）七百張量產能跑到哪一步、缺什麼才能開始真素材？
（5）`VisualRef`／def 的 `visual` 欄位定義了沒、有沒有人消費？

## 結論

通了中間一段，兩端都沒接。

閘三（後處理六步）與閘四的自動檢查已完工且位元可重現，是唯一有程式碼的部分。
閘一（色盤、參考圖、prompt 模板）與閘二（生成、sidecar、批次）零程式碼，
閘一整條卡在使用者。Godot 端沒有任何吃圖檔的程式碼，畫面是硬寫色表塗的 8×8 色塊。
`VisualRef` 在 core 定義並從 toml 讀進來了，但全專案沒有一處消費它，
連 bridge 都不送出去。端到端：真素材 0 張、能把圖送上畫面的程式路徑 0 條。

## 現況

### 1 `tools/art/`（M9.2／M9.2b 產物）

- 追蹤檔 8 個：`art_pipeline.py` 575 行、`verify_acceptance.py` 411 行、
  `tests/test_art_pipeline.py` 331 行、`tests/conftest.py` 9 行、
  `placeholder_palette.json` 71 行，加 `README.md`／`requirements.txt`
  （Pillow==12.3.0、pytest==9.0.2）／`.gitignore`。
- CLI 只有 3 個子命令：`process`、`gate`、`generate-placeholder-palette`
  （`tools/art/art_pipeline.py:498,512,518`）。沒有 batch 子命令，一次一張。
- 輸入：單張圖 + 外部色盤 JSON（`colors` 陣列、`#RRGGBB`、不得重複，
  `art_pipeline.py:58-72`）。輸出：單張 PNG，不是圖集，一次寫一個檔
  （`art_pipeline.py:362-400`），路徑 = def id 點換斜線加 `.png`
  （`art_pipeline.py:111-115`）。
- 常數：格線預設 `(64, 64)`、色盤外門檻 `0.01`（`art_pipeline.py:18,19`）。
- 隨附色盤是佔位：以 0/85/170/255 三通道笛卡兒積產生固定 64 色
  （`art_pipeline.py:74-84`），檔內自帶 `placeholder-only-not-project-palette` 警語。
- 測試：`tests/test_art_pipeline.py` 有 18 個 `test_` 函式。⚠ 本機 `python3`
  沒有 pytest，這 18 條本次沒實跑；Pillow 12.3.0 有裝。
- `verify_acceptance.py` 本次實跑，退出碼 0。它自帶反向對照：5 個正向 gate 案例
  （clean=0，尺寸／色盤／alpha／命名各 exit 2）、4 個「停用該規則」對照組全部變 exit 0、
  接縫度量 95.625 → 0.0（跳過修補時維持 95.625）、terrain 四方向旋轉
  `[0, 270, 180, 90]` 且四張雜湊相同、`detection_power_audit` 13 項自評
  （`verify_acceptance.py:311-325`）。

### 2 四道閘的狀態

| 閘 | 狀態 | 佐證 |
|---|---|---|
| 一 錨定 | **0**，且卡使用者 | 色盤是公式佔位；參考圖組與 prompt 模板全專案不存在 |
| 二 生成 | **0** | 無 batch、無 sidecar 參數記錄、無模型選型；`tools/art/` 只有後處理 |
| 三 後處理 | **六步全做，位元可重現** | `process_asset` `art_pipeline.py:376-386` 依序跑完六步 |
| 四 驗收 | **1/3** | 只有「自動檢查」（`run_gate`，`art_pipeline.py:446-491`）；接觸表與實機拼貼無腳本 |

卡使用者的在 [WAIT_USER.md](../../WAIT_USER.md)「M9 呈現：素材本體只有你能做」：色盤
48～64 實際色值 + 10～20 張參考圖組、prompt 前後綴 + 反向 prompt、選哪個生成模型。
`design/presentation/art-pipeline.md` 末段「待細化」還列了 6 項未定。

### 3 素材怎麼進 Godot——沒有路

- **全 repo 沒有 `art/` 素材目錄**（`find` 只命中 `tools/art`）；`out/` 下的 PNG
  是 worldgen 除錯輸出，`godot/artifacts/` 是驗收截圖。
- `godot/` 沒有任何 `TileSet`／`TileMapLayer`／`load()` 貼圖；唯一的貼圖是程式生成的
  `ImageTexture`（`godot/main.gd:244,247`）。
- 佔位素材的真身是**硬寫的色表**：`godot/region_debug_palette.gd:4` `TILE_SIZE := 8`
  （不是規格的 64），`:7` TERRAIN 5 筆、`:15` FEATURE 8 筆、`:26` RELIEF 3 筆、
  `:32` EDGE 8 筆，人手維護，非工具生成。
- 缺色已經在咬人：terrain def 共 **7 個**（`data/terrain.toml` 5 + `data/biomes.toml` 2，
  兩份都載入見 `core/rules/ruleset_load_defs.cpp:36-39`），TERRAIN 色表只有 5 筆——
  `terrain.taiga`／`terrain.steppe` 落到洋紅 fallback `Color8(255, 0, 255)`
  （`godot/region_debug_renderer.gd:30`），而 `data/biomes.toml:18,23` 的規則確實產出這兩種。

### 4 七百張的量產流程

估算表在 `design/presentation/art-specs.md:31-44`（合計約 700）。極限是**手上若有一張已生成的圖，
可以逐張 `process` 再逐張 `gate`**；不能做批次、圖集打包、`_{00..03}` 變體、
autotile 連接、生成參數 sidecar。要開始真素材，前置是閘一那三件（全在 WAIT_USER）。

### 5 `VisualRef` 定義了、沒人消費

- 定義：`core/rules/def_types.h:112-117`，註解明寫「core 不解讀、原樣交給顯示層」；
  帶此欄位的 def 有 5 種：Terrain／Relief／Feature／Edge／Ground
  （`def_types.h:127,138,149,161,172`）。
- 讀取：`core/rules/toml_read.h:105` `require_string(table, "visual", ...)`——**必填**。
- 資料：`data/*.toml` 共 **51 筆** `visual`
  （edges 26、feature 9、ground 6、terrain 5、relief 3、biomes 2）。
- 消費：**零**。`grep` 整個 `bridge/` 對 `visual` 無命中；snapshot 只送
  `terrain_ids`／`relief_ids`（`bridge/aetheria_core.cpp:343,344,420,421`）。

## 差距

1. **閘一整條未開始**，且不是工程能自解的——真色盤沒定之前生成任何素材都要重做。
2. **閘二零程式碼**：沒有批次、沒有 sidecar 參數記錄，`art-pipeline.md` 要求的
   「局部重生成可重現」無從談起。
3. **Godot 端沒有吃素材的程式碼**，這是「畫面上看到」那端最大的斷點。
4. **兩套命名並存且互不相通**：`data/*.toml` 的 `visual` 是斜線形（`"terrain/taiga"`），
   `tools/art` 走 def id 點換斜線。工具不讀、核心不解讀、顯示層拿不到——目前是死重。
5. **art-specs 說「不必維護任何索引表」，實際維護了兩張**：toml 的 `visual` 必填欄位，
   與 `region_debug_palette.gd` 的硬寫色表；後者已經漏掉 2 種地形。
6. **輸出形狀對不上規格**：規格要圖集、`_{00..03}` 變體、autotile 16／47 連接與
   64×96 角色；工具只輸出無後綴單張 PNG，`--size` 要呼叫端自己給。
7. **閘四缺 2/3**：接觸表與實機拼貼都寫在設計文件裡，沒有腳本或場景。
8. **`object` 錯向主光只警告不擋**（`art_pipeline.py:246-250`，gate 無 `fail` 模式），
   而「光源統一」是 art-specs 自稱最容易被眼睛察覺的一條。
9. **18 條 pytest 在本機跑不起來**（無 pytest），可重複驗證只剩 `verify_acceptance.py`。

## 牽扯到的部份

- `tools/art/` 三支 Python（獨立於 CMake／CTest，見 `tools/art/README.md`）。
- `core/rules/def_types.h` 的 `VisualRef` 與 5 種 def、`core/rules/toml_read.h:105`
  的必填解析、`data/` 下 6 個 toml 的 51 筆 `visual`。
- `bridge/aetheria_core.cpp` 的 snapshot 打包（要送 `visual` 就會動到）。
- `godot/region_debug_palette.gd`、`region_debug_renderer.gd`、`main.gd`、
  `project.godot`（目前無任何貼圖 import 設定）。
- 判準：M8.2 的「關掉 core 重開畫面完全一致」機器證據（ImageMagick `AE = 0`）
  是量在**現在這套色塊渲染**上的；換成真素材會重新定義它的量測對象。
- [WAIT_USER.md](../../WAIT_USER.md) 的 M9 三項；`design/presentation/art-pipeline.md` 與
  `design/presentation/art-specs.md` 各自的「待細化」清單。
- `~/repo/game_dev/my_godot_assists/`（art-specs 點名的四個可複用元件，本次未查證）。
