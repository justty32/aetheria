// cross_zone.cpp：執行期跨 Local zone 查詢與具 rollback 的實體搬移交易。

#include <aetheria/runtime/cross_zone.h>

#include "core/world/region_movement.h"
#include "core/zone/zone.h"
#include "core/zone/zone_manager.h"

#include <algorithm>
#include <array>
#include <limits>
#include <map>

#include <entt/core/type_info.hpp>

namespace aetheria::runtime {
namespace {

using MigratableComponents =
    entt::type_list<world::StableId, world::RegionPosition, world::MovementPoints,
                    world::RegionMoveCommand, LocalPosition>;
using UidIndex = std::map<std::uint64_t, entt::entity>;

[[nodiscard]] constexpr bool valid_tile(local::LocalXY tile) noexcept {
    return tile.x < local::kLocalWidth && tile.y < local::kLocalHeight;
}

[[nodiscard]] constexpr std::size_t tile_index(local::LocalXY tile) noexcept {
    return static_cast<std::size_t>(tile.y) * local::kLocalWidth + tile.x;
}

[[nodiscard]] const local::LocalTiles* ground_layer(const zone::Zone& value) noexcept {
    const auto* payload = std::get_if<zone::LocalPayload>(&value.payload);
    if (payload == nullptr) {
        return nullptr;
    }
    const auto found = payload->layers.find(0);
    return found == payload->layers.end() || !found->second.valid_layout()
               ? nullptr
               : &found->second;
}

[[nodiscard]] std::optional<UidIndex> make_uid_index(const zone::Zone& value) {
    UidIndex result;
    for (const auto entity : value.reg.view<const world::StableId>()) {
        const auto uid = value.reg.get<const world::StableId>(entity).uid;
        if (!result.emplace(uid, entity).second) {
            return std::nullopt;
        }
    }
    return result;
}

template <typename... Components>
[[nodiscard]] bool has_only_migratable_components(const entt::registry& registry,
                                                  entt::entity entity,
                                                  entt::type_list<Components...>) {
    const std::array allowed{entt::type_hash<Components>::value()...};
    for (const auto [id, storage] : registry.storage()) {
        if (storage.contains(entity) &&
            std::ranges::find(allowed, id) == allowed.end()) {
            return false;
        }
    }
    return true;
}

template <typename... Components>
void copy_components(const entt::registry& source, entt::entity source_entity,
                     entt::registry& destination, entt::entity destination_entity,
                     entt::type_list<Components...>) {
    const auto copy_one = [&]<typename Component>() {
        if (source.all_of<Component>(source_entity)) {
            destination.emplace<Component>(destination_entity,
                                           source.get<const Component>(source_entity));
        }
    };
    (copy_one.template operator()<Components>(), ...);
}

}  // namespace

std::optional<TileView> CrossZoneRuntime::peek_tile(zone::ZoneKey key,
                                                    local::LocalXY tile) const noexcept {
    if (!valid_tile(tile)) {
        return std::nullopt;
    }
    const auto handle = manager_.get(key);
    if (!handle.has_value()) {
        return std::nullopt;
    }
    std::optional<TileView> result;
    static_cast<void>(manager_.with(*handle, [&](const zone::Zone& value) {
        const auto* layer = ground_layer(value);
        if (layer == nullptr) {
            return;
        }
        const auto index = tile_index(tile);
        result = TileView{layer->ground[index], layer->overlay[index],
                          layer->occupant[index], layer->light[index]};
    }));
    return result;
}

std::optional<EdgeView> CrossZoneRuntime::peek_edge(
    zone::ZoneKey key, local::LocalXY tile,
    spatial::BoundarySide direction) const noexcept {
    const auto direction_index = static_cast<std::size_t>(direction);
    if (!valid_tile(tile) || direction_index >= 4U) {
        return std::nullopt;
    }
    const auto handle = manager_.get(key);
    if (!handle.has_value()) {
        return std::nullopt;
    }
    std::optional<EdgeView> result;
    static_cast<void>(manager_.with(*handle, [&](const zone::Zone& value) {
        const auto* layer = ground_layer(value);
        if (layer != nullptr) {
            result = EdgeView{layer->edges[tile_index(tile) * 4U + direction_index]};
        }
    }));
    return result;
}

std::optional<entt::entity> CrossZoneRuntime::resolve(EntityRef reference) const noexcept {
    const auto handle = manager_.get(reference.zone);
    if (!handle.has_value()) {
        return std::nullopt;
    }
    std::optional<entt::entity> result;
    static_cast<void>(manager_.with(*handle, [&](const zone::Zone& value) {
        const auto found = value.uid_index.find(reference.uid);
        if (found != value.uid_index.end() && value.reg.valid(found->second)) {
            const auto* stable = value.reg.try_get<const world::StableId>(found->second);
            if (stable != nullptr && stable->uid == reference.uid) {
                result = found->second;
            }
        }
    }));
    return result;
}

bool CrossZoneRuntime::migrate_entity(zone::ZoneKey from, entt::entity entity,
                                      zone::ZoneKey to, local::LocalXY at) {
    if (from == to || zone::level_of(from) != zone::ZoneLevel::Local ||
        zone::level_of(to) != zone::ZoneLevel::Local || !valid_tile(at)) {
        return false;
    }
    const auto source_handle = manager_.get(from);
    if (!source_handle.has_value()) {
        return false;
    }
    auto destination_handle = manager_.get(to);
    if (!destination_handle.has_value()) {
        if (manager_.is_ticking()) {
            return false;
        }
        try {
            if (!manager_.load(to)) {
                return false;
            }
        } catch (...) {
            return false;
        }
        destination_handle = manager_.get(to);
        if (!destination_handle.has_value()) {
            return false;
        }
    }

    const std::array handles{*source_handle, *destination_handle};
    bool migrated{};
    const bool borrowed = manager_.with_many(handles, [&](std::span<zone::Zone* const> zones) {
        auto& source = *zones[0];
        auto& destination = *zones[1];
        if (!source.reg.valid(entity) || ground_layer(destination) == nullptr ||
            !has_only_migratable_components(source.reg, entity, MigratableComponents{})) {
            return;
        }

        try {
            auto source_index = make_uid_index(source);
            auto destination_index = make_uid_index(destination);
            if (!source_index.has_value() || !destination_index.has_value()) {
                return;
            }
            source.uid_index.swap(*source_index);
            destination.uid_index.swap(*destination_index);

            const auto staged = destination.reg.create();
            bool destination_indexed{};
            std::uint64_t uid{};
            try {
                copy_components(source.reg, entity, destination.reg, staged,
                                MigratableComponents{});
                destination.reg.emplace_or_replace<LocalPosition>(staged, at);
                if (const auto* stable = source.reg.try_get<const world::StableId>(entity)) {
                    uid = stable->uid;
                    if (!destination.uid_index.emplace(uid, staged).second) {
                        destination.reg.destroy(staged);
                        return;
                    }
                    destination_indexed = true;
                }

                source.reg.destroy(entity);
                if (destination_indexed) {
                    source.uid_index.erase(uid);
                }
                if (destination.touch_count != std::numeric_limits<std::uint64_t>::max()) {
                    ++destination.touch_count;
                }
                migrated = true;
            } catch (...) {
                if (destination_indexed) {
                    destination.uid_index.erase(uid);
                }
                if (destination.reg.valid(staged)) {
                    destination.reg.destroy(staged);
                }
            }
        } catch (...) {
            return;
        }
    });
    return borrowed && migrated;
}

}  // namespace aetheria::runtime
