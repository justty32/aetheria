#include "sim/gen_commands.h"

#include "core/worldgen/region_generator.h"
#include "sim/pgm_writer.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace aetheria::sim {

namespace {

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

}  // namespace

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

}  // namespace aetheria::sim
