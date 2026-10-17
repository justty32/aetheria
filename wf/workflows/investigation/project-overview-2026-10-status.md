# project-overview-2026-10 — 全專案現況盤點（2026-10-17）

← [investigation](README.md)｜[SESSION-LOG](../../SESSION-LOG.md)｜[WAIT_USER](../../WAIT_USER.md)

## 問題

使用者原話：「整理當前狀況，並且給出建議」。拆成：
1. 實作停在哪？v23／432 綠是否與程式碼一致？
2. 活世界規劃（roadmap、spec）完成幾成？恢復實作還缺什麼？
3. 懸案（inbox 任務書、地形、WAIT_USER、SESSION-LOG open）是否仍真實？

## 結論

實作**停在 M10.3a，與文件一致**；規劃**文件齊、零定案**（roadmap 整份是草案、D01–D20 無一裁定）；
所有懸案都卡在**同一個使用者裁定：roadmap 輪次重排**。另揭出工作流本身三處漏洞（見差距 1–3）。

## 現況

### 實作

- 存檔版本 `kSaveFormatVersion = 23`（`core/serialize/zone_codec.h:13`）；寫死 23 的 `static_assert` 3 處：
  `tests/zone/zone_codec_test.cpp:103`、`tests/site/site_building_mapping_cold_load_test.cpp:30`、
  `tests/site/site_observation_persistence_compatibility_test.cpp:93`。
- `ctest -N` 432 個（gtest 416＋`add_test` 16）；本次**未實跑**。近 30 筆 commit 全是拆檔與文件，無玩法改動。
- M10.3b 未開工（全 repo grep 無 `RulesetPatcher`）；範圍見 [runtime-injection-inject](../plan/runtime-injection-inject.md):39-83。
- 交接書繼承缺陷：A-1 多角色槽重放雜湊不一致、A-2 被拒命令無日誌、B-1/B-2 崩潰點救不回
  （[波 2 交接](../plan/runtime-injection-wave2-handoff.md):68-80）。

### 規劃與懸案

- roadmap 標題即「2026-09-09 草案」，第 5 步「形成動工規格」未做、R0–R6「目前不執行」
  （[roadmap](../plan/living-world/roadmap-2026-09.md):1,27,39）。spec 總覽 33 份子規格皆草案，
  D01–D20 無定案（`design/spec/overview/spec-world.md:7,23-55`）。
- inbox：M9.0（98 行）、M9.1（120 行）未開工；core/bridge/godot 無 mainline 程式碼，godot 無音訊程式碼。
- 地形：無人接手；濕度 41.8% 格飽和於 65535 是 swamp 過量根因（[TERRAIN-HANDOFF](TERRAIN-HANDOFF.md):16-25）。
- WAIT_USER 實有 5 條 open（色盤、prompt 模板、音訊閘一、observer mark、音景人耳判準）。

## 差距

1. **開場檢查漏數 WAIT_USER**：AGENTS.md 開場用 `grep -c '^- \['`，WAIT_USER 條目是 `- ⚠` 開頭，永遠數到 0。
2. **KNOWN-TRAPS 過期**：`KNOWN-TRAPS.md:46`「遊戲不會存檔」，但 bridge 已有 `save_game`（`bridge/aetheria_core_session.cpp:75`）。
3. **計畫檔失準**：`runtime-injection-inject.md:16` 引用 `playable_session.cpp:915-995`（檔案現僅 157 行）；
   `:37`、`:83` 寫「bump → v23」，與交接書 v23→24 矛盾。
4. **M9.0 任務書過期**：寫存檔「20 → 21」（`m9-0-mainline-victory.md:39`）、基準「M8-INT-7 合併後」（:6）。
5. **「能動但不像真的」歸因不精確**：SESSION-LOG 掛在 mark 下，但 M5 四處症狀與 mark 無關，WAIT_USER 無獨立條目。
6. `design/outline.md:42` 中地圖寫「城建經營 或 SRPG 戰略」，roadmap 為回合制方陣（roadmap:9）。
7. spec 與 R 輪無對應欄；mark/root 未指定輪次（roadmap-coverage.json:97）。

覆核註記：子 agent 曾疑 `cmake/check_world_hash_boundary.cmake` 是假綠燈；實讀為**邊界 grep 檢查**、非雜湊比對，**不成立**。

## 牽扯到的部份

AGENTS.md 開場指令、KNOWN-TRAPS、WAIT_USER、SESSION-LOG；`wf/inbox/m9-0-*.md`、`m9-1-*.md`；
`wf/workflows/plan/runtime-injection-inject.md`、波 2 交接；roadmap／roadmap-coverage／spec 總覽；
`design/outline.md`；定版輪的 3 處 `static_assert`。
