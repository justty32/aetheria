#include "core/serialize/normalized_state_hash.h"

#include <aetheria/runtime/cross_zone.h>

#include "core/site/site_build_loop.h"
#include "core/site/site_lifecycle.h"
#include "core/local/local_navigation.h"
#include "core/world/army_state.h"
#include "core/world/region_movement.h"
#include "core/world/named_fate.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/serialize/normalized_state_hash_detail.h"

namespace aetheria::serialize::detail {

void hash_city_build_state(std::uint64_t& hash, const zone::Zone& zone,
                           const rules::Ruleset& ruleset) {
    const auto states = zone.reg.view<const site::CityBuildState>();
    if (states.size() > 1U) {
        throw std::runtime_error{"正規化雜湊遇到多個 CityBuildState"};
    }
    hash_scalar(hash, static_cast<std::uint64_t>(states.size()));
    if (states.empty()) {
        return;
    }
    const auto& state = states.get<const site::CityBuildState>(*states.begin());
    if (zone::level_of(zone.key) != zone::ZoneLevel::Site ||
        !site::valid_city_build_state(state, ruleset)) {
        throw std::runtime_error{"正規化雜湊遇到無效 CityBuildState"};
    }
    auto buildings = state.buildings;
    std::ranges::sort(buildings, [](const auto& left, const auto& right) {
        return std::tie(left.definition_id, left.origin.y, left.origin.x) <
               std::tie(right.definition_id, right.origin.y, right.origin.x);
    });
    hash_scalar(hash, static_cast<std::uint64_t>(buildings.size()));
    for (const auto& building : buildings) {
        hash_string(hash, building.definition_id);
        hash_scalar(hash, building.origin.x);
        hash_scalar(hash, building.origin.y);
    }
    auto pending = state.pending;
    std::ranges::sort(pending, [](const auto& left, const auto& right) {
        return std::tie(left.definition_id, left.origin.y, left.origin.x,
                        left.remaining_hours) <
               std::tie(right.definition_id, right.origin.y, right.origin.x,
                        right.remaining_hours);
    });
    hash_scalar(hash, static_cast<std::uint64_t>(pending.size()));
    for (const auto& construction : pending) {
        hash_string(hash, construction.definition_id);
        hash_scalar(hash, construction.origin.x);
        hash_scalar(hash, construction.origin.y);
        hash_scalar(hash, construction.remaining_hours);
    }
    hash_scalar(hash, state.economy.population);
    hash_scalar(hash, state.economy.food_stock);
    hash_scalar(hash, state.economy.production_stock);
    hash_scalar(hash, state.economy.population_micro_remainder);
    hash_scalar(hash, state.economy.satisfaction);
    hash_scalar(hash,
                static_cast<std::uint64_t>(state.migration.destroyed_objects.size()));
    for (const auto& object : state.migration.destroyed_objects) {
        hash_scalar(hash, object.kind);
        hash_string(hash, object.definition_id);
        hash_scalar(hash, object.former_coordinate.x);
        hash_scalar(hash, object.former_coordinate.y);
        hash_scalar(hash, object.persistent_type);
        hash_scalar(hash, object.persistent_state);
        hash_scalar(hash, object.aging_seconds);
        hash_scalar(hash, object.remaining_hours);
    }
    hash_scalar(hash, static_cast<std::uint64_t>(state.migration.events.size()));
    for (const auto& event : state.migration.events) {
        hash_scalar(hash, event.old_skeleton_hash);
        hash_scalar(hash, event.new_skeleton_hash);
        hash_scalar(hash, event.retained);
        hash_scalar(hash, event.relocated);
        hash_scalar(hash, event.destroyed);
        hash_string(hash, event.narrative);
    }
}

} // namespace aetheria::serialize::detail
