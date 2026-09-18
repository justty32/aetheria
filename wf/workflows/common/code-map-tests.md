# code-map — 測試與護欄

← [code-map](code-map.md)

## `tests/` 的檔名與共用支援

GoogleTest 依核心領域分目錄。`<主題>_test.cpp` 是該主題測試，
`<主題>_<子題>_test.cpp` 依子題分檔；舊主題檔可能仍保留部分案例，不是 include 門面。
`*_test_support.h` 只供所屬目錄 fixture／helper，不能當核心公開介面。
`tests/support/` 放跨目錄 ruleset fixture、邊界支援與固定暖機、min-of-N 效能量測工具。
來源清單在 `cmake/targets_*.cmake`，入口是 `cmake/targets_tests.cmake`；
完整執行方式見 [testing](../testing.md)。

## 世界與規則

`tests/rules/` 驗載入、錯誤約束與 codec、力量位階、四屬性／檢定／傷害、
魔法／信仰／血統及不可替代性、戰鬥公式／校準、外交資料擴充與錯誤速率。
以 ruleset、individual_rules、power_sources、combat 等前綴定位子題。

`tests/world/` 驗 SoA 格資料、移動成本／A*／旬回合、命運分布／持久化、
戰鬥升降格／生命週期、外交有向矩陣／期限／戰爭／厭戰／和談與決定性。
faction_ai 群驗目標切換、性格、知識誤判、LOD 成本與無偏、均勢、代管、連鎖參戰及效能。

`tests/worldgen/` 驗地形／氣候函式、決定論、參數隔離、輸出、效能與重測；
history 群驗身分／災變／回饋／隔離，人文群驗城市／首都／道路、影響力／治理釋回／勢力量測，
portal 與晚期隔離群驗通道及後段生成邊界。

## 執行期與持久化

`tests/runtime/` 驗三層進退、上層回寫、親自／代管校準、磁碟冷存冷讀與 pending 拒存；
history_log 群驗鏈連續性、崩潰恢復、重放、uid 與亂數軌跡；character_save 群驗雙角色、
世界身分及門共享；save_raws 群驗世界槽自帶 TOML 的隔離與竄改拒絕。

`tests/zone/` 驗 key／生命週期、記憶體與磁碟 store 共用契約、manifest、codec、
ZoneManager 時間與 callback、跨 zone 查詢／搬移／rollback，外交存檔及 legacy 版本拒讀。
`tests/serialize/` 驗 EnTT registry 往返與壓測；`tests/time/` 驗曆法邊界與往返。
`tests/sim/` 驗世界級正規化 hash 的跨歷史、磁碟列舉、負向控制與錯誤路徑。

## 分層與擴展

`tests/site/` 驗投影、骨架／城區／荒野、展開／駐留／歸約、城建、戰鬥、存檔 hash 與效能；
`tests/local/` 驗邊界、A／B／C 生成、探索、Local 歸約、戰鬥與地城。
具體不變式與量測預算分別見 [Site](code-map-site.md) 與 [Local](code-map-local.md)。

`tests/narrative/` 驗五種湧現任務、運糧／清剿歸約、命運模板與事件 feed；
`tests/script/` 驗 Lua 決定論、同 seed 世界 hash、原子重載與 min-of-5 效能。

## Compile-fail 與目錄可見性

`tests/compile_fail/` 是刻意不可編譯的見證，不加入一般成功編譯清單。
`cmake/check_*isolation.cmake` 檢查失敗原因，不能只接受任意非零退出碼。
core 隔離另掃編譯命令，確認全部核心 translation units 不含 godot-cpp。

AI 受限 target 只見 `core/ai/include/`；worldgen 見 repo 根目錄與 AI 公開 include，
不見 `core/runtime/include/` 的跨 zone 公開入口；script 受限使用者只見
`core/script/include/`，不見 Context 內部真值接點。Script 護欄另驗解除限制後必須編譯成功。
各 target 的責任見對應 code map，不可把實作移到可見更多目錄的 target 來繞過限制。
`cmake/check_deity_region_mediation.cmake` 另驗神祇不能繞過信徒事件 API 直接取得 Region 狀態，
這是 API 護欄，與 include 不可見的失敗不同。

## 獨立負向控制執行檔

`tests/zone/cross_zone_half_move_negative.cpp` 對應
`aetheria_cross_zone_half_move_negative`；`tests/rules/combat_ratio_domain_negative.cpp`
對應 `aetheria_combat_ratio_domain_negative`，分別故意破壞搬移原子性與戰鬥倍率定義域。

`tests/world/named_fate_order_negative.cpp` 對應 `aetheria_named_fate_order_negative`；
`tests/world/combat_scaling_negative.cpp` 對應 `aetheria_combat_scaling_negative`，
分別驗命運順序與三層戰鬥護欄。

`tests/local/local_combat_negative.cpp` 對應 `aetheria_local_combat_negative`，
故意讓 Local 自選入口及繞過 M5 FOV；CTest 的 `LocalCombatNegative.RequiredFailures`
檢查 East→North 入口錯置及穿牆攻擊 0→1。
這些執行檔預期失敗，由 `cmake/check_*negative.cmake` 檢查退出碼與證據輸出後才算通過，
不能直接混進普通成功測試或把故意失敗改綠。
