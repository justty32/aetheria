DONE — M10.0b 的 8 條 golden 雜湊已改為關係斷言，6 條演算法常數已註明，負向控制與 417 條 CTest 均完成

## 變更摘要

- `region_determinism_test.cpp`：三個 reference seed 不再對寫死結果，改為正序／反序各自生成一次後按 seed 比較 skeleton／tiles hash；同時涵蓋同參數重跑與建構順序無關。
- site／local boundary：不再對 golden profile hash，改比較相鄰兩側各自投影所得的 profile hash。
- `file_zone_store_test.cpp`：存前 hash 改與真正重開 store 後的 cold-load hash 比較；fixture 依 store 合約先寫 root、目標 zone 與 manifest。
- 6 個 FNV-1a 64-bit offset basis 保留，逐處註明是演算法常數而非 golden 輸出。

## 14 條常數分類（任務開始時行號）

| # | 檔案:行 | 常數 | 分類 | 處置 |
|---:|---|---:|---|---|
| 1 | `tests/worldgen/region_determinism_test.cpp:51` | 5754128893694281728 | golden | 移除；seed 515151 的 skeleton 改為正序／反序建構 hash 相等 |
| 2 | `tests/worldgen/region_determinism_test.cpp:52` | 8963508752675768512 | golden | 移除；seed 515151 的 tiles 改為正序／反序建構 hash 相等 |
| 3 | `tests/worldgen/region_determinism_test.cpp:53` | 17267498220237237745 | golden | 移除；seed 12345 的 skeleton 改為正序／反序建構 hash 相等 |
| 4 | `tests/worldgen/region_determinism_test.cpp:54` | 14515705340403023595 | golden | 移除；seed 12345 的 tiles 改為正序／反序建構 hash 相等 |
| 5 | `tests/worldgen/region_determinism_test.cpp:55` | 793007085422239155 | golden | 移除；seed 424242 的 skeleton 改為正序／反序建構 hash 相等 |
| 6 | `tests/worldgen/region_determinism_test.cpp:56` | 3836747774080975080 | golden | 移除；seed 424242 的 tiles 改為正序／反序建構 hash 相等 |
| 7 | `tests/site/site_wilderness_boundary_test.cpp:65` | 3093732465121518141 | golden | 移除；比較 west／east 各自投影的 boundary profile hash |
| 8 | `tests/local/local_boundary_test.cpp:63` | 3316258571901256250 | golden | 移除；比較 west／east 各自投影的 boundary profile hash |
| 9 | `tests/support/boundary_profile.h:11` | 14695981039346656037 | algorithm | 保留；FNV-1a 64-bit offset basis，加合法性註解 |
| 10 | `tests/sim/world_hash_test_support.h:95` | 14695981039346656037 | algorithm | 保留；FNV-1a 64-bit offset basis，加合法性註解 |
| 11 | `tests/worldgen/road_network_test.cpp:27` | 14695981039346656037 | algorithm | 保留；FNV-1a 64-bit offset basis，加合法性註解 |
| 12 | `tests/worldgen/history_layer_test.cpp:25` | 14695981039346656037 | algorithm | 保留；FNV-1a 64-bit offset basis，加合法性註解 |
| 13 | `tests/worldgen/history_layer_test.cpp:38` | 14695981039346656037 | algorithm | 保留；FNV-1a 64-bit offset basis，加合法性註解 |
| 14 | `tests/worldgen/influence_test_support.h:21` | 14695981039346656037 | algorithm | 保留；FNV-1a 64-bit offset basis，加合法性註解 |

沒有「拿不準」項。seed、ZoneKey、事件基底、splitmix 增量等其他 `UINT64_C` 是測試輸入或不同領域的演算法常數，不是本次盤點的世界／區域 hash 輸出快照。

## grep 證明

退場的 8 個值逐一掃描：

```text
$ grep -rnE 'UINT64_C\((5754128893694281728|8963508752675768512|17267498220237237745|14515705340403023595|793007085422239155|3836747774080975080|3093732465121518141|3316258571901256250)\)' tests
# 無輸出；0 matches
```

大十進位常數直接拿來做 `EXPECT_EQ` 的掃描亦為零：

```text
$ grep -rnE 'EXPECT_EQ\([^\n]*UINT64_C\([0-9]{15,}\)' tests
# 無輸出；0 matches
```

剩餘六處 `14695981039346656037` 的前一行全都是：

```text
// FNV-1a 64-bit offset basis: an algorithm constant, not a golden output hash.
```

位置為 `road_network_test.cpp:28`、`history_layer_test.cpp:26,40`、`boundary_profile.h:12`、`influence_test_support.h:22`、`world_hash_test_support.h:96`。

## 負向控制

暫時把 `core/serialize/zone_encode.cpp` 的 `saved_tick` 編碼值改成 `last_saved_tick + 1`，以 `--parallel 2` 跑 cold-load 關係測試：

```text
1/1 Test #279: FileZoneStore.RoundTripPreservesCanonicalBitsAndEntityCount ... Failed
tests/zone/file_zone_store_test.cpp:76: Failure
Expected equality of these values:
  cold_loaded_hash  Which is: 17223397907053261071
  before_hash       Which is: 11168667888992201474
0% tests passed, 1 tests failed out of 1
```

已用 patch 還原 `zone_encode.cpp`；`git diff -- core/serialize/zone_encode.cpp` 無輸出。還原後針對性 4/4 綠，完整結果：

```text
100% tests passed out of 417
Total Test time (real) = 46.88 sec
```

完整建置使用 `cmake --build build --parallel 2` 且成功。CTest 數量未減：目前 417；原始碼 `TEST`／`TEST_P` 宣告在 HEAD 與目前皆為 407。

## 模組與依賴（原則九）

- 新模組：零。
- 新共用 helper／新測試檔／CMake 變更：零。
- 產品依賴：零新增；`core/`、`bridge/`、`godot/` 最終均未修改。
- 測試依賴：只沿用既有 `FileZoneStore`、`SaveManifest`、`Zone`、hash 與 fixture API；依賴方向仍是 tests → core。
