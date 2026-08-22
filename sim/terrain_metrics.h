#pragma once

// terrain_metrics.h 提供可重複呼叫的 Region 地形分布與海岸量測入口。

#include "core/rules/ruleset.h"

#include <cstdint>

namespace aetheria::sim {

int run_terrain_metrics(const aetheria::rules::Ruleset &ruleset,
                        std::uint64_t seed, std::uint32_t region_id,
                        std::int16_t latitude_degrees);

} // namespace aetheria::sim
