# 設計文件入口

本層只管設計全局與導引；子題進所屬資料夾，不再平鋪。每份文件上限 8 KB、繁體中文；資料夾變雜時依職責分層，每層 README 兼索引或另設 INDEX。

## 先讀

1. [outline.md](outline.md)：全局常數與遊戲定位。
2. [principles.md](principles.md)：貫穿全案的原則。
3. [glossary.md](glossary.md)：術語仲裁。
4. [INDEX.md](INDEX.md)：選下一層領域入口。

## 要往哪裡走

| 想知道 | 下一層入口 |
|---|---|
| 最新活世界規格 | [spec/](spec/README.md)：狀態、依賴、各領域草案 |
| 三層地圖怎麼玩 | [maps/](maps/README.md) |
| 核心、zone、存檔與建置 | [architecture/](architecture/README.md)；改基礎設施前先讀該層 medps 繼承說明 |
| 時間、LOD、交接與事件 | [simulation/](simulation/README.md) |
| 世界與地圖怎麼生成 | [generation/](generation/README.md) |
| 戰鬥、外交、內容與追加 | [rules/](rules/README.md) |
| 美術、素材與音景 | [presentation/](presentation/README.md) |
| 里程碑與歷史規劃狀態 | [milestones.md](milestones.md) |

## 寫文件

- 葉文件指向本層入口，並引用原則／術語；橫向依賴可直接連結，不複製規則正文。
- 全局常數只在大綱維護，其他文件引用，不重複定值。
- 未定事項明標草案／待裁定；搬移不代表批准，文件完整不代表實作完成。
- 外部研究附實際來源及證據界限；每層只列下一層的文件／資料夾。
