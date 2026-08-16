#pragma once

// 道路成本／路徑內部 helper，供 road_network.cpp 的 generate_roads 使用，
// 從 civilization_generator.cpp 拆出。

#include "core/worldgen/region_generator.h"

#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace aetheria::worldgen::detail {

[[nodiscard]] std::optional<rules::EdgeId>
compound_edge(const rules::CivilizationRules& civilization, rules::EdgeId river,
              rules::EdgeId road);

[[nodiscard]] std::optional<rules::EdgeId>
underlying_river(const rules::CivilizationRules& civilization, rules::EdgeId edge,
                 const rules::Ruleset& ruleset);

[[nodiscard]] std::size_t directed_offset(std::size_t from, std::size_t to, std::uint32_t width);

[[nodiscard]] std::vector<std::size_t>
find_engineering_path(const world::RegionTiles& tiles, const RiverStageOutput& rivers,
                      std::size_t start, std::size_t goal, const rules::Ruleset& ruleset,
                      const rules::CivilizationRules& civilization);

}  // namespace aetheria::worldgen::detail
