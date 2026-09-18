#include "core/api/version.h"
#include "core/runtime/save_raws.h"
#include "core/rules/ruleset.h"
#include "core/serialize/zone_codec.h"
#include "core/site/site_materialize.h"
#include "core/time/tick.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/zone_manager.h"
#include "sim/gen_commands.h"
#include "sim/core_probe_detail.h"
#include "sim/local_viewer.h"
#include "sim/site_viewer.h"
#include "sim/terrain_metrics.h"
#include "sim/world_hash.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include <CLI/CLI.hpp>

namespace {

[[nodiscard]] std::uint64_t parse_seed(const std::string& value) {
    std::size_t parsed{};
    const auto result = std::stoull(value, &parsed, 0);
    if (parsed != value.size()) {
        throw std::invalid_argument{"site_seed 必須是十進位或 0x 開頭的十六進位整數"};
    }
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    CLI::App app{"Aetheria headless core probe"};
    std::int64_t requested_tick = 31'104'000;
    std::string save_directory;
    std::string data_directory{AETHERIA_DEFAULT_DATA_DIR};
    std::uint64_t generation_seed{};
    std::uint32_t generation_region_id{};
    std::int16_t generation_latitude{35};
    std::uint16_t erosion_iterations{12};
    std::int16_t biome_moisture_bias{};
    std::string dump_stages;
    std::string world_hash_directory;
    std::string replay_directory;
    std::uint32_t verify_iterations{100};
    std::string viewer_site_seed{"0x5A17"};
    std::string viewer_zoning{"residential"};
    std::string viewer_z{"all"};
    std::string viewer_kind{"city"};
    std::string viewer_output{"out"};
    app.add_option("--tick", requested_tick, "額外換算的 Tick（秒）");
    app.add_option("--save-dir", save_directory, "跨程序 zone 存檔目錄");
    app.add_option("--data-dir", data_directory, "Ruleset TOML 資料目錄");
    auto* gen = app.add_subcommand("gen", "程序生成除錯工具");
    gen->require_subcommand(1);
    auto* gen_region = gen->add_subcommand("region", "生成一個 Region 並輸出階段雜湊");
    gen_region->add_option("--seed", generation_seed, "世界 seed")->required();
    gen_region->add_option("--region", generation_region_id, "Region id");
    gen_region->add_option("--erosion-iterations", erosion_iterations, "固定熱力侵蝕次數");
    gen_region->add_option("--biome-moisture-bias", biome_moisture_bias,
                           "biome 查表前的水氣偏移（階段 6 隔離探針）");
    gen_region->add_option("--dump-stages", dump_stages, "十二階段 PGM 輸出目錄");
    auto* gen_terrain_metrics =
        gen->add_subcommand("terrain-metrics", "量測地形直方圖、飽和率與海岸品質");
    gen_terrain_metrics->add_option("--seed", generation_seed, "世界 seed")->required();
    gen_terrain_metrics->add_option("--region", generation_region_id, "Region id");
    gen_terrain_metrics->add_option("--latitude", generation_latitude, "Region 中心緯度");
    auto* gen_local = gen->add_subcommand("local", "生成 Local 診斷 PNG");
    gen_local->add_option("--site-seed", viewer_site_seed,
                          "Site seed（十進位或 0x 十六進位）");
    gen_local->add_option("--zoning", viewer_zoning,
                          "residential、commercial、open、mine、dungeon 或 ruin");
    gen_local->add_option("--z", viewer_z, "現有 z 層號或 all");
    gen_local->add_option("--output", viewer_output, "PNG 輸出目錄");
    auto* gen_site = gen->add_subcommand("site", "生成 Site 診斷 PNG");
    gen_site->add_option("--site-seed", viewer_site_seed,
                         "Site seed（十進位或 0x 十六進位）");
    gen_site->add_option("--kind", viewer_kind, "city 或 wilderness");
    gen_site->add_option("--output", viewer_output, "PNG 輸出目錄");
    auto* gen_verify = gen->add_subcommand("verify", "重複生成並驗證同 seed 決定論");
    gen_verify->add_option("--seed", generation_seed, "起始世界 seed")->required();
    gen_verify->add_option("--iterations", verify_iterations, "驗證 seed 數量");
    auto* verify = app.add_subcommand("verify", "存檔狀態驗證工具");
    verify->require_subcommand(1);
    auto* verify_world_hash =
        verify->add_subcommand("world-hash", "計算跨 zone 正規化世界狀態雜湊");
    verify_world_hash->add_option("save_dir", world_hash_directory, "存檔槽目錄")->required();
    auto* replay = app.add_subcommand("replay", "從基底 raws 與歷史日誌重建世界");
    replay->add_option("save_dir", replay_directory, "存檔槽目錄")->required();
    CLI11_PARSE(app, argc, argv);

    if (*verify_world_hash) {
        return aetheria::sim::run_world_hash(world_hash_directory);
    }
    if (*replay) {
        return aetheria::sim::run_replay(replay_directory);
    }

    std::filesystem::path rules_directory{data_directory};
    if (!save_directory.empty() && !*gen) {
        const std::filesystem::path slot_directory{save_directory};
        std::error_code error;
        const bool has_manifest =
            std::filesystem::exists(slot_directory / "manifest.bin", error);
        if (error) {
            throw std::runtime_error{"無法檢查存檔 manifest：" + error.message()};
        }
        if (!has_manifest) {
            static_cast<void>(
                aetheria::runtime::copy_save_raws(rules_directory, slot_directory));
        }
        rules_directory = aetheria::runtime::save_raws_directory(slot_directory);
    }
    const auto ruleset = aetheria::rules::RulesetLoader::load(rules_directory);

    if (*gen_region) {
        return aetheria::sim::run_gen_region(ruleset, generation_seed, generation_region_id,
                                             erosion_iterations, biome_moisture_bias, dump_stages);
    }
    if (*gen_terrain_metrics) {
        return aetheria::sim::run_terrain_metrics(ruleset, generation_seed, generation_region_id,
                                                  generation_latitude);
    }
    if (*gen_local) {
        return aetheria::sim::run_gen_local(ruleset, parse_seed(viewer_site_seed), viewer_zoning,
                                            viewer_z, viewer_output);
    }
    if (*gen_site) {
        return aetheria::sim::run_gen_site(ruleset, parse_seed(viewer_site_seed), viewer_kind,
                                           viewer_output);
    }
    if (*gen_verify) {
        return aetheria::sim::run_gen_verify(ruleset, generation_seed, verify_iterations);
    }
    return aetheria::sim::detail::run_core_probe(ruleset, requested_tick, save_directory);
}
