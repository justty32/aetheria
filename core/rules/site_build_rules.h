#pragma once

// site_build_rules.h 定義城建循環的建築數值、相鄰效果與人口成長規則。

#include "core/rules/def_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aetheria::rules {

struct CityAdjacencyBonus {
    CityBuildingDefId neighbor{};
    std::int16_t production_per_hour{};
    std::int16_t satisfaction{};
};

struct CityBuildingDef {
    std::string id;
    std::uint8_t width{};
    std::uint8_t height{};
    std::uint16_t construction_hours{};
    std::uint32_t housing_capacity{};
    std::uint16_t food_per_hour{};
    std::uint16_t production_per_hour{};
    std::int16_t satisfaction{};
    std::vector<CityAdjacencyBonus> adjacency;
};

struct SiteBuildRules {
    std::uint16_t base_growth_basis_points_per_xun{};
    std::uint16_t people_supported_per_food{};
    std::uint8_t base_satisfaction{};
    bool loaded{};
};

}  // namespace aetheria::rules
