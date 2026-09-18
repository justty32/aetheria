// site_build_economy_validation.cpp：持久城建狀態、工地與遷移歷史驗證。

#include "core/site/site_build_loop.h"

#include <algorithm>

namespace aetheria::site {

bool valid_city_build_state(const CityBuildState& state,
                            const rules::Ruleset& ruleset) noexcept {
    if (state.economy.satisfaction > 100U) {
        return false;
    }
    const auto valid_building = [&](std::string_view id, SiteXY origin) {
        const auto found = ruleset.find_city_building(id);
        const auto* definition = found.has_value() ? ruleset.city_building(*found) : nullptr;
        return definition != nullptr &&
               static_cast<std::uint32_t>(origin.x) + definition->width <= kSiteWidth &&
               static_cast<std::uint32_t>(origin.y) + definition->height <= kSiteHeight;
    };
    const auto valid_destroyed = [&](const SiteMigrationDestroyedObject& object) {
        if (object.former_coordinate.x >= kSiteWidth ||
            object.former_coordinate.y >= kSiteHeight ||
            object.kind > SiteMigrationObjectKind::PendingConstruction) {
            return false;
        }
        if (object.kind == SiteMigrationObjectKind::PersistentBuilding) {
            return object.definition_id.empty() &&
                   object.persistent_type <= BuildingType::CivicSquare &&
                   object.persistent_state <= BuildingState::Ruined;
        }
        return ruleset.find_city_building(object.definition_id).has_value();
    };
    const auto valid_event = [](const SiteMigrationEvent& event) {
        return event.old_skeleton_hash != event.new_skeleton_hash &&
               !event.narrative.empty();
    };
    return std::ranges::all_of(state.buildings, [&](const CityBuilding& building) {
               return valid_building(building.definition_id, building.origin);
           }) &&
           std::ranges::all_of(state.pending, [&](const PendingConstruction& construction) {
               return construction.remaining_hours != 0 &&
                      valid_building(construction.definition_id, construction.origin);
           }) &&
           std::ranges::all_of(state.migration.destroyed_objects, valid_destroyed) &&
           std::ranges::all_of(state.migration.events, valid_event);
}

}  // namespace aetheria::site
