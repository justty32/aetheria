// `sim gen region` / `sim gen verify` 子命令的實作。
#pragma once

#include "core/rules/ruleset.h"

#include <cstdint>
#include <filesystem>

namespace aetheria::sim {

int run_gen_region(const aetheria::rules::Ruleset& ruleset, std::uint64_t seed,
                   std::uint32_t region_id, std::uint16_t erosion_iterations,
                   std::int16_t biome_moisture_bias, const std::filesystem::path& dump_directory);

int run_gen_verify(const aetheria::rules::Ruleset& ruleset, std::uint64_t seed,
                   std::uint32_t iterations);

}  // namespace aetheria::sim
