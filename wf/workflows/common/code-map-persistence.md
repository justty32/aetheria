# code-map — Zone 與序列化

← [code-map](code-map.md)

## `core/zone/` — 生命週期與 store

本目錄定義 ZoneKey、LOD、各層 payload、生命週期與存檔契約。
`zone_store.*` 是共用契約與記憶體 store，`file_zone_store.*` 是磁碟版；
`save_manifest*` 分 schema 與 I/O，處理 manifest codec、磁碟存取與 zstd。
`zone_manager.*` 管 zone 取得、生命週期與重展開 callback。
LocalPayload 的 layers 是同一 Local zone 的 z → LocalTiles 程序層集合。

## `core/serialize/` — 位元流與正規化雜湊

`core/serialize/zone_codec.h` 是 zone codec 入口：現行格式 v23，預設只解現行版；
v14／v15 只供 LegacyFixture 模式，不能據此放寬正常 store 的版本拒讀。
`zone_encode*.cpp` 分編碼與驗證，`zone_decode*.cpp` 分總解碼、def 重映射與 registry 還原；
`zone_diplomacy_*.cpp` 處理 root 外交、情報與 AI 狀態的編解碼及 def id 重映射。
`*_detail.h` 是 codec 群內的檢查與跨檔介面。

外交尾段自 v17 在 v15 基礎尾端追加 FactionTruth、KnowledgeRecord、FactionMindState；
讀 v15 fixture 時三者保持明確缺席。這是相容性測試語意，現行版本仍以 codec 入口為準。

`normalized_state_hash_<component群>.cpp` 依城建、外交、命運、payload、單位等群組計算，
由 `normalized_state_hash*.cpp` 的總入口組合跨歷史正規化 hash；不可用 registry 建立順序
取代持久語意。Region 部隊的 faction／power／玩家控制權威狀態歸 world，
Local 已開門 edge 集合 LocalDoorState 歸 local navigation。

## Snapshot 順序護欄

`core/serialize/all_components.h` 的 AllComponents 是 EnTT snapshot 白名單，
**新增可存檔 component 只能加尾端**：順序就是位元流順序，漏登記會靜默漏存，插中間會讓舊存檔錯位。
同目錄 registry codec 使用此清單處理 snapshot；portal codec 保存 Region 通道資料。

世界槽／角色檔／歷史交易的上層編排見 [Runtime](code-map-runtime.md)，
世界級 canonical zone 掃描見 [Sim](code-map-adapters.md)，往返與版本拒讀見
[測試詳圖](code-map-tests.md)。
