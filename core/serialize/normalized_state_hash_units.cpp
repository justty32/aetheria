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

namespace {

template <typename Component> void require_stable_ids(const zone::Zone& zone) {
    for (const auto entity : zone.reg.view<const Component>()) {
        if (!zone.reg.all_of<world::StableId>(entity)) {
            throw std::runtime_error{"權威 unit component 缺少 StableId"};
        }
    }
}

} // namespace

void hash_units(std::uint64_t& hash, const zone::Zone& zone) {
    require_stable_ids<world::RegionPosition>(zone);
    require_stable_ids<world::MovementPoints>(zone);
    require_stable_ids<world::RegionMoveCommand>(zone);
    require_stable_ids<world::ArmyState>(zone);
    require_stable_ids<runtime::LocalPosition>(zone);
    std::vector<std::pair<std::uint64_t, entt::entity>> entities;
    for (const auto entity : zone.reg.view<const world::StableId>()) {
        entities.emplace_back(zone.reg.get<const world::StableId>(entity).uid, entity);
    }
    std::ranges::sort(entities);
    hash_scalar(hash, static_cast<std::uint64_t>(entities.size()));
    std::uint64_t previous{};
    bool has_previous{};
    for (const auto [stable_id, entity] : entities) {
        if (has_previous && stable_id == previous) {
            throw std::runtime_error{"正規化雜湊遇到重複 StableId：" + std::to_string(stable_id)};
        }
        previous = stable_id;
        has_previous = true;
        hash_scalar(hash, stable_id);

        const auto* position = zone.reg.try_get<const world::RegionPosition>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(position != nullptr));
        if (position != nullptr) {
            hash_scalar(hash, position->z);
            hash_scalar(hash, position->tile.x);
            hash_scalar(hash, position->tile.y);
        }
        const auto* local_position = zone.reg.try_get<const runtime::LocalPosition>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(local_position != nullptr));
        if (local_position != nullptr) {
            hash_scalar(hash, local_position->tile.x);
            hash_scalar(hash, local_position->tile.y);
        }
        const auto* points = zone.reg.try_get<const world::MovementPoints>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(points != nullptr));
        if (points != nullptr) {
            hash_scalar(hash, points->current);
            hash_scalar(hash, points->per_xun);
        }
        const auto* command = zone.reg.try_get<const world::RegionMoveCommand>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(command != nullptr));
        if (command != nullptr) {
            hash_scalar(hash, command->target.x);
            hash_scalar(hash, command->target.y);
            hash_scalar(hash, static_cast<std::uint8_t>(command->collected));
        }
        const auto* army = zone.reg.try_get<const world::ArmyState>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(army != nullptr));
        if (army != nullptr) {
            hash_scalar(hash, army->faction);
            hash_scalar(hash, army->power);
            hash_scalar(hash, static_cast<std::uint8_t>(army->player_controlled));
        }
    }
}

} // namespace aetheria::serialize::detail
