#pragma once

// stage_dump.h 提供十二階段 Region PGM 除錯輸出。

#include "core/worldgen/region_generator.h"

#include <filesystem>

namespace aetheria::sim {

void dump_region_stages(const worldgen::RegionBuildResult& result,
                        const std::filesystem::path& directory);

}  // namespace aetheria::sim
