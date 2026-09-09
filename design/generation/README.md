# 世界與地圖生成

← [上層入口](../README.md)

生成管線、跨邊一致性，以及各地圖層的地形／文明／場所生成。先讀管線與接邊，再進各生成器。

本 README 兼本層索引，只列本資料夾文件；跨領域依賴在各篇正文連結。

| 文件 | 職責 |
|---|---|
| [gen-pipeline.md](gen-pipeline.md) | 程序生成：共通契約 |
| [edge-consistency.md](edge-consistency.md) | 接邊一致性 |
| [worldgen-terrain.md](worldgen-terrain.md) | L1 生成：地形與氣候 |
| [worldgen-climate.md](worldgen-climate.md) | L1 生成：氣候（階段 4） |
| [worldgen-civ.md](worldgen-civ.md) | L1 生成：文明、道路與出入口 |
| [worldgen-history.md](worldgen-history.md) | L1 生成：歷史層（階段 8） |
| [worldgen-factions.md](worldgen-factions.md) | L1 生成：勢力起始（階段 12） |
| [sitegen-city.md](sitegen-city.md) | L2 生成：城區 |
| [sitegen-wild.md](sitegen-wild.md) | L2 生成：荒野、廢墟與海域 |
| [localgen.md](localgen.md) | L3 生成：Local |
