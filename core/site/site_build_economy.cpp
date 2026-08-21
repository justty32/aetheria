// site_build_economy.cpp：城建每小時產出、相鄰效果、人口與工地進度結算。

#include "core/site/site_build_loop_detail.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace aetheria::site::build_detail {
namespace {

constexpr std::int64_t kHoursPerXun = 240;

[[nodiscard]] bool rectangles_adjacent(SiteXY a, const rules::CityBuildingDef& a_def, SiteXY b,
                                       const rules::CityBuildingDef& b_def) noexcept {
    const auto ax1 = static_cast<std::uint32_t>(a.x) + a_def.width;
    const auto ay1 = static_cast<std::uint32_t>(a.y) + a_def.height;
    const auto bx1 = static_cast<std::uint32_t>(b.x) + b_def.width;
    const auto by1 = static_cast<std::uint32_t>(b.y) + b_def.height;
    const bool vertical_overlap = a.y < by1 && b.y < ay1;
    const bool horizontal_overlap = a.x < bx1 && b.x < ax1;
    return ((ax1 == b.x || bx1 == a.x) && vertical_overlap) ||
           ((ay1 == b.y || by1 == a.y) && horizontal_overlap);
}

struct HourlyEconomy {
    std::uint64_t capacity{};
    std::uint64_t food{};
    std::int64_t production{};
    std::int64_t satisfaction{};
    std::uint64_t adjacency_triggers{};
};

[[nodiscard]] HourlyEconomy measure_hour(const CityBuildState& state,
                                         const rules::Ruleset& ruleset) {
    HourlyEconomy result;
    result.satisfaction = ruleset.site_build_rules().base_satisfaction;
    for (std::size_t index = 0; index < state.buildings.size(); ++index) {
        const auto& building = state.buildings[index];
        const auto& definition = require_definition(ruleset, building.definition_id);
        result.capacity += definition.housing_capacity;
        result.food += definition.food_per_hour;
        result.production += definition.production_per_hour;
        result.satisfaction += definition.satisfaction;
        for (std::size_t neighbor_index = 0; neighbor_index < state.buildings.size();
             ++neighbor_index) {
            if (index == neighbor_index) {
                continue;
            }
            const auto& neighbor = state.buildings[neighbor_index];
            const auto neighbor_id = ruleset.find_city_building(neighbor.definition_id);
            if (!neighbor_id.has_value() ||
                !rectangles_adjacent(building.origin, definition, neighbor.origin,
                                     require_definition(ruleset, neighbor.definition_id))) {
                continue;
            }
            for (const auto& bonus : definition.adjacency) {
                if (bonus.neighbor == *neighbor_id) {
                    result.production += bonus.production_per_hour;
                    result.satisfaction += bonus.satisfaction;
                    ++result.adjacency_triggers;
                }
            }
        }
    }
    if (result.production < 0) {
        throw std::runtime_error{"城建相鄰效果讓每小時產出成為負值"};
    }
    result.satisfaction = std::clamp<std::int64_t>(result.satisfaction, 0, 100);
    return result;
}

void advance_population(CityEconomy& economy, const HourlyEconomy& hourly,
                        const rules::SiteBuildRules& rules, SiteAdvanceReport& report) {
    if (economy.population == 0) {
        return;
    }
    const auto food_needed =
        (static_cast<std::uint64_t>(economy.population) + rules.people_supported_per_food - 1U) /
        rules.people_supported_per_food;
    const auto supported =
        hourly.food >= food_needed
            ? UINT64_C(100)
            : hourly.food * rules.people_supported_per_food * 100U / economy.population;
    const auto modifier = static_cast<std::int64_t>(supported) + hourly.satisfaction - 100;
    auto delta = static_cast<std::int64_t>(economy.population) *
                 rules.base_growth_basis_points_per_xun * modifier / kHoursPerXun;
    if (delta > 0 && economy.population >= hourly.capacity) {
        delta = 0;
    }
    economy.population_micro_remainder += delta;
    constexpr std::int64_t population_scale = 1'000'000;
    auto whole = economy.population_micro_remainder / population_scale;
    economy.population_micro_remainder %= population_scale;
    if (whole > 0) {
        const auto headroom = hourly.capacity > economy.population
                                  ? hourly.capacity - economy.population
                                  : std::uint64_t{};
        const auto bounded_headroom =
            std::min<std::uint64_t>(headroom, std::numeric_limits<std::int64_t>::max());
        whole = std::min<std::int64_t>(whole, static_cast<std::int64_t>(bounded_headroom));
        economy.population += static_cast<std::uint32_t>(whole);
        report.population_births += static_cast<std::uint32_t>(whole);
    } else if (whole < 0) {
        const auto deaths = std::min<std::uint64_t>(static_cast<std::uint64_t>(-whole),
                                                    economy.population);
        economy.population -= static_cast<std::uint32_t>(deaths);
        report.population_deaths += static_cast<std::uint32_t>(deaths);
    }
}

}  // namespace

std::uint32_t simulate_hour(CityBuildState& state, const rules::Ruleset& ruleset,
                            SiteAdvanceReport& report) {
    const auto hourly = measure_hour(state, ruleset);
    if (hourly.food > std::numeric_limits<std::uint64_t>::max() - state.economy.food_stock ||
        static_cast<std::uint64_t>(hourly.production) >
            std::numeric_limits<std::uint64_t>::max() - state.economy.production_stock) {
        throw std::overflow_error{"城建每小時產出累積溢位"};
    }
    state.economy.food_stock += hourly.food;
    state.economy.production_stock += static_cast<std::uint64_t>(hourly.production);
    state.economy.satisfaction = static_cast<std::uint8_t>(hourly.satisfaction);
    report.food_produced += hourly.food;
    report.production_produced += static_cast<std::uint64_t>(hourly.production);
    report.adjacency_bonus_triggers += hourly.adjacency_triggers;
    advance_population(state.economy, hourly, ruleset.site_build_rules(), report);

    std::uint32_t completed{};
    for (auto& construction : state.pending) {
        if (construction.remaining_hours == 0) {
            throw std::runtime_error{"城建工地 remaining_hours 不得為 0"};
        }
        --construction.remaining_hours;
    }
    for (auto iterator = state.pending.begin(); iterator != state.pending.end();) {
        if (iterator->remaining_hours != 0) {
            ++iterator;
            continue;
        }
        state.buildings.push_back({std::move(iterator->definition_id), iterator->origin});
        iterator = state.pending.erase(iterator);
        ++completed;
    }
    state.economy.hours_into_xun =
        static_cast<std::uint16_t>((state.economy.hours_into_xun + 1U) % kHoursPerXun);
    return completed;
}

}  // namespace aetheria::site::build_detail

namespace aetheria::site {

bool valid_city_build_state(const CityBuildState& state,
                            const rules::Ruleset& ruleset) noexcept {
    if (state.economy.hours_into_xun >= 240U || state.economy.satisfaction > 100U) {
        return false;
    }
    const auto valid_building = [&](std::string_view id, SiteXY origin) {
        const auto found = ruleset.find_city_building(id);
        const auto* definition = found.has_value() ? ruleset.city_building(*found) : nullptr;
        return definition != nullptr &&
               static_cast<std::uint32_t>(origin.x) + definition->width <= kSiteWidth &&
               static_cast<std::uint32_t>(origin.y) + definition->height <= kSiteHeight;
    };
    return std::ranges::all_of(state.buildings, [&](const CityBuilding& building) {
               return valid_building(building.definition_id, building.origin);
           }) &&
           std::ranges::all_of(state.pending, [&](const PendingConstruction& construction) {
               return construction.remaining_hours != 0 &&
                      valid_building(construction.definition_id, construction.origin);
           });
}

}  // namespace aetheria::site
