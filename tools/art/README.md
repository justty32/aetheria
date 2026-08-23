# Aetheria 美術後處理工具

這裡只有確定性的後處理與入庫檢查，不含生成模型或正式素材。隨附的
`placeholder_palette.json` 是由固定公式產生的測試資料，**不是專案色盤**；正式使用前必須換成
使用者核定、同格式的 48～64 色 JSON。

## 安裝與測試

```sh
python3 -m pip install -r tools/art/requirements.txt
python3 -m pytest -p no:cacheprovider tools/art/tests -q
python3 tools/art/verify_acceptance.py
```

這組 pytest 獨立執行，不掛進 CMake／CTest。工具固定使用 Pillow 12.3.0，讓 PNG 編碼器版本也
納入可重現環境。

## 六步處理

```sh
python3 tools/art/art_pipeline.py process \
  --input incoming/grass.png \
  --asset-root staged/art/tiles \
  --def-id terrain.grassland \
  --kind terrain \
  --palette path/to/project_palette.json
```

處理順序固定如下：

1. 依 alpha 或 `--background R,G,B` 去背，alpha 硬化為 0/255；裁出可見範圍後對齊指定格線。
2. 以固定 4×4 Bayer 矩陣做有序抖動，再映射至外部色盤的最近色；不做誤差擴散。
3. 以整數亮度梯度判斷四象限主光，將整張圖旋轉至左上主光。無有效梯度時不旋轉。
4. `object` 將 alpha 輪廓內緣改成固定線寬與色盤第一色；`terrain` 以最近內部色移除輪廓色。
5. `terrain` 將相對邊成對合併並再次選色盤最近色，讓外框接縫度量成為零。
6. def id 以點換斜線並加 `.png`；例如 `terrain.grassland` 寫到
   `<asset-root>/terrain/grassland.png`。

預設格線為 64×64；64×96 角色可加 `--size 64x96`。物件等比例縮小並置中，地形則填滿格線。
若輸入沒有 alpha，呼叫端應明確提供背景色與可選的 `--background-tolerance`。標準輸出會列出
光源判定、旋轉角、修補前後接縫度量與 SHA-256。

## 入庫閘

manifest 範例：

```json
{
  "assets": [
    {"def_id": "terrain.grassland", "size": [64, 64]},
    {"def_id": "actor.guard", "size": [64, 96]}
  ]
}
```

```sh
python3 tools/art/art_pipeline.py gate \
  --asset-root staged/art/tiles \
  --manifest manifest.json \
  --palette path/to/project_palette.json
```

以下任一情況會印出素材路徑與違規項目並以退出碼 2 拒絕：尺寸不符、可見像素中色盤外
比例大於 1%、存在 1～254 的半透明 alpha、透明像素仍帶非零 RGB，或實際相對路徑與
manifest 的 def id 不符。
恰好 1% 色盤外像素依規格仍通過；超過才拒絕。

## 已知界線

- 去背是依既有 alpha 或指定背景色的確定性規則，不是語意分割。
- 梯度只能判定明暗主軸；多重光源或無方向性光線會回報 `undetermined`。
- 旋轉會連主體朝向一起改變，因此有方向語意的建築、角色與物件仍需人工檢視。
- 邊界修補只保證相對外框相同；材質跨邊的連續性仍要看接觸表與實機拼貼。
