#include "core/api/version.h"
#include "core/rules/ruleset.h"
#include "core/serialize/zone_codec.h"
#include "core/time/tick.h"
#include "core/worldgen/region_generator.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/zone_manager.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

namespace {

void write_pgm(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height,
               const std::vector<std::uint8_t>& pixels) {
    if (pixels.size() != static_cast<std::size_t>(width) * height) {
        throw std::runtime_error{"PGM pixel 數與尺寸不符"};
    }
    std::ofstream stream{path, std::ios::binary};
    if (!stream.is_open()) {
        throw std::runtime_error{"無法建立 stage dump：" + path.string()};
    }
    stream << "P5\n" << width << ' ' << height << "\n255\n";
    stream.write(reinterpret_cast<const char*>(pixels.data()),
                 static_cast<std::streamsize>(pixels.size()));
    if (!stream.good()) {
        throw std::runtime_error{"寫入 stage dump 失敗：" + path.string()};
    }
}

void print_generation(const aetheria::worldgen::RegionBuildResult& result,
                      const aetheria::world::RegionTiles& tiles,
                      std::chrono::steady_clock::duration elapsed) {
    std::cout << "plate_hash=" << aetheria::worldgen::hash_stage(result.plates) << '\n'
              << "height_hash=" << aetheria::worldgen::hash_stage(result.height) << '\n'
              << "erosion_hash=" << aetheria::worldgen::hash_stage(result.erosion) << '\n'
              << "climate_hash=" << aetheria::worldgen::hash_stage(result.climate) << '\n'
              << "river_hash=" << aetheria::worldgen::hash_stage(result.rivers) << '\n'
              << "biome_hash=" << aetheria::worldgen::hash_stage(result.biome) << '\n'
              << "feature_hash=" << aetheria::worldgen::hash_stage(result.features) << '\n'
              << "city_hash=" << aetheria::worldgen::hash_stage(result.cities) << '\n'
              << "road_hash=" << aetheria::worldgen::hash_stage(result.roads) << '\n'
              << "skeleton_hash=" << aetheria::worldgen::hash_skeleton(result.skeleton) << '\n'
              << "tiles_hash=" << aetheria::worldgen::hash_tiles(tiles) << '\n'
              << std::fixed << std::setprecision(3)
              << "land_percent=" << aetheria::worldgen::land_fraction(result.skeleton) * 100.0
              << '\n'
              << "land_connected="
              << (aetheria::worldgen::land_is_single_component(result.skeleton) ? "true" : "false")
              << '\n'
              << "elapsed_ms=" << std::chrono::duration<double, std::milli>{elapsed}.count()
              << '\n';
}

int run_gen_region(const aetheria::rules::Ruleset& ruleset, std::uint64_t seed,
                   std::uint32_t region_id, std::uint16_t erosion_iterations,
                   std::int16_t biome_moisture_bias, const std::filesystem::path& dump_directory) {
    aetheria::worldgen::RegionGenerationConfig config;
    config.erosion.iterations = erosion_iterations;
    config.biome.moisture_bias = biome_moisture_bias;
    const auto start = std::chrono::steady_clock::now();
    const auto result = aetheria::worldgen::build_skeleton(
        aetheria::worldgen::RegionSlowVariables{region_id, 128, 96}, seed, ruleset, config);
    const auto tiles =
        aetheria::worldgen::populate(result.skeleton, aetheria::worldgen::RegionFastVariables{});
    const auto elapsed = std::chrono::steady_clock::now() - start;

    if (!dump_directory.empty()) {
        std::filesystem::create_directories(dump_directory);
        write_pgm(dump_directory / "01-plates.pgm", result.plates.width, result.plates.height,
                  aetheria::worldgen::grayscale(result.plates));
        write_pgm(dump_directory / "02-height.pgm", result.height.width, result.height.height,
                  aetheria::worldgen::grayscale(result.height));
        write_pgm(dump_directory / "03-erosion.pgm", result.erosion.width, result.erosion.height,
                  aetheria::worldgen::grayscale(result.erosion));
        write_pgm(dump_directory / "04-climate.pgm", result.climate.width, result.climate.height,
                  aetheria::worldgen::grayscale(result.climate));
        write_pgm(dump_directory / "05-rivers.pgm", result.rivers.width, result.rivers.height,
                  aetheria::worldgen::grayscale(result.rivers));
        write_pgm(dump_directory / "06-biome.pgm", result.biome.width, result.biome.height,
                  aetheria::worldgen::grayscale(result.biome));
        write_pgm(dump_directory / "07-features.pgm", result.features.width, result.features.height,
                  aetheria::worldgen::grayscale(result.features));
        write_pgm(dump_directory / "08-cities.pgm", result.cities.width, result.cities.height,
                  aetheria::worldgen::grayscale(result.cities));
        write_pgm(dump_directory / "09-roads.pgm", result.roads.width, result.roads.height,
                  aetheria::worldgen::grayscale(result.roads));
    }
    print_generation(result, tiles, elapsed);
    return 0;
}

int run_gen_verify(const aetheria::rules::Ruleset& ruleset, std::uint64_t seed,
                   std::uint32_t iterations) {
    if (iterations == 0) {
        throw std::invalid_argument{"gen verify 的 iterations 必須大於 0"};
    }
    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t iteration = 0; iteration < iterations; ++iteration) {
        const auto current_seed = aetheria::worldgen::splitmix64(seed ^ iteration);
        const aetheria::worldgen::RegionSlowVariables slow{iteration, 128, 96};
        const auto first = aetheria::worldgen::build_skeleton(slow, current_seed, ruleset);
        const auto second = aetheria::worldgen::build_skeleton(slow, current_seed, ruleset);
        const auto first_tiles =
            aetheria::worldgen::populate(first.skeleton, aetheria::worldgen::RegionFastVariables{});
        const auto second_tiles = aetheria::worldgen::populate(
            second.skeleton, aetheria::worldgen::RegionFastVariables{});
        if (aetheria::worldgen::hash_stage(first.plates) !=
                aetheria::worldgen::hash_stage(second.plates) ||
            aetheria::worldgen::hash_stage(first.height) !=
                aetheria::worldgen::hash_stage(second.height) ||
            aetheria::worldgen::hash_stage(first.erosion) !=
                aetheria::worldgen::hash_stage(second.erosion) ||
            aetheria::worldgen::hash_stage(first.climate) !=
                aetheria::worldgen::hash_stage(second.climate) ||
            aetheria::worldgen::hash_stage(first.rivers) !=
                aetheria::worldgen::hash_stage(second.rivers) ||
            aetheria::worldgen::hash_stage(first.biome) !=
                aetheria::worldgen::hash_stage(second.biome) ||
            aetheria::worldgen::hash_stage(first.features) !=
                aetheria::worldgen::hash_stage(second.features) ||
            aetheria::worldgen::hash_stage(first.cities) !=
                aetheria::worldgen::hash_stage(second.cities) ||
            aetheria::worldgen::hash_stage(first.roads) !=
                aetheria::worldgen::hash_stage(second.roads) ||
            aetheria::worldgen::hash_skeleton(first.skeleton) !=
                aetheria::worldgen::hash_skeleton(second.skeleton) ||
            aetheria::worldgen::hash_tiles(first_tiles) !=
                aetheria::worldgen::hash_tiles(second_tiles)) {
            throw std::runtime_error{"gen verify 在 iteration " + std::to_string(iteration) +
                                     " 發現同 seed 漂移"};
        }
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::cout << "verified_iterations=" << iterations << '\n'
              << std::fixed << std::setprecision(3)
              << "elapsed_ms=" << std::chrono::duration<double, std::milli>{elapsed}.count()
              << '\n';
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    CLI::App app{"Aetheria headless core probe"};
    std::int64_t requested_tick = 31'104'000;
    std::string save_directory;
    std::string data_directory{AETHERIA_DEFAULT_DATA_DIR};
    std::uint64_t generation_seed{};
    std::uint32_t generation_region_id{};
    std::uint16_t erosion_iterations{12};
    std::int16_t biome_moisture_bias{};
    std::string dump_stages;
    std::uint32_t verify_iterations{100};
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
    gen_region->add_option("--dump-stages", dump_stages, "九階段 PGM 輸出目錄");
    auto* gen_verify = gen->add_subcommand("verify", "重複生成並驗證同 seed 決定論");
    gen_verify->add_option("--seed", generation_seed, "起始世界 seed")->required();
    gen_verify->add_option("--iterations", verify_iterations, "驗證 seed 數量");
    CLI11_PARSE(app, argc, argv);

    const auto ruleset = aetheria::rules::RulesetLoader::load(data_directory);

    if (*gen_region) {
        return run_gen_region(ruleset, generation_seed, generation_region_id, erosion_iterations,
                              biome_moisture_bias, dump_stages);
    }
    if (*gen_verify) {
        return run_gen_verify(ruleset, generation_seed, verify_iterations);
    }

    std::cout << "Aetheria core " << aetheria::core_version() << '\n';
    const std::array ticks{std::int64_t{0}, std::int64_t{864'000}, requested_tick};
    for (const auto raw_tick : ticks) {
        const auto date = aetheria::time::to_date(aetheria::time::Tick{raw_tick});
        std::cout << "tick=" << raw_tick << " -> year=" << date.year
                  << " season=" << static_cast<unsigned>(date.season)
                  << " month=" << static_cast<unsigned>(date.month)
                  << " xun=" << static_cast<unsigned>(date.xun) << '\n';
    }

    const auto region = aetheria::zone::child_key(aetheria::zone::kRootZone, 1, 0);
    const auto site = aetheria::zone::child_key(region, 4, 7);
    const auto local = aetheria::zone::child_key(site, 12, 9);

    const auto run_zone_probe = [&](aetheria::zone::ZoneStore& zone_store, bool load_existing,
                                    aetheria::zone::FileZoneStore* file_store) {
        aetheria::zone::ZoneManager zone_manager{zone_store};
        for (const auto key : {region, site, local}) {
            if (load_existing) {
                static_cast<void>(zone_manager.require(key));
            } else {
                const auto handle = zone_manager.materialize(key);
                static_cast<void>(zone_manager.with(handle, [&](aetheria::zone::Zone& zone) {
                    if (aetheria::zone::level_of(key) == aetheria::zone::ZoneLevel::Region) {
                        auto& layers = std::get<aetheria::zone::RegionPayload>(zone.payload).layers;
                        auto& tiles =
                            layers.emplace(0, aetheria::world::RegionTiles{2, 2}).first->second;
                        tiles.temperature.at(0) = static_cast<std::uint8_t>(17U);
                    }
                }));
            }
        }
        zone_manager.tick(aetheria::time::Tick{requested_tick});
        zone_manager.save_all();
        if (file_store != nullptr) {
            auto manifest = file_store->manifest().value_or(aetheria::zone::SaveManifest{});
            manifest.world_seed = UINT64_C(0xA37E12A);
            manifest.now = aetheria::time::Tick{requested_tick};
            file_store->write_manifest(manifest);
        }

        std::cout << "zone tree:\n";
        for (const auto key : zone_manager.loaded_keys()) {
            const auto level = static_cast<unsigned>(aetheria::zone::level_value_of(key));
            const auto handle = zone_manager.get(key);
            std::uint64_t hash{};
            static_cast<void>(zone_manager.with(*handle, [&](const aetheria::zone::Zone& zone) {
                hash = aetheria::serialize::persistent_state_hash(zone, ruleset);
            }));
            std::cout << "  level=" << level << " key=" << aetheria::zone::value_of(key)
                      << " parent=" << aetheria::zone::value_of(aetheria::zone::parent_of(key))
                      << " hash=" << hash << '\n';
        }
    };

    if (save_directory.empty()) {
        aetheria::zone::InMemoryZoneStore zone_store{ruleset};
        run_zone_probe(zone_store, false, nullptr);
    } else {
        aetheria::zone::FileZoneStore zone_store{save_directory, ruleset};
        const bool load_existing = zone_store.manifest().has_value();
        run_zone_probe(zone_store, load_existing, &zone_store);
    }
}
