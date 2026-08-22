#pragma once

// world_observation_rules.h：世界觀測初值、湧現任務門檻與完成效果的資料規則。

#include <cstdint>

namespace aetheria::rules {

struct WorldObservationRules {
    std::uint16_t initial_garrison_coverage{};
    std::uint16_t initial_patrol_coverage{};
    std::uint16_t initial_bandit_pressure{};
    std::uint16_t initial_refugee_pressure{};
    std::uint16_t bandit_minimum_order{};
    std::uint16_t bandit_pressure_reduction{};
    std::uint64_t food_delivery_required{};
    std::uint16_t dungeon_minimum_depth{};
    bool loaded{};
};

}  // namespace aetheria::rules
