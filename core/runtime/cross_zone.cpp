#include <aetheria/runtime/cross_zone.h>

#include "core/world/region_movement.h"
#include "core/zone/zone.h"
#include "core/zone/zone_manager.h"

#include <algorithm>
#include <array>
#include <limits>
#include <map>

#include <entt/core/type_info.hpp>

#include "core/runtime/cross_zone_detail.h"

namespace aetheria::runtime {
using namespace detail;

namespace {

[[nodiscard]] constexpr std::size_t tile_index(local::LocalXY tile) noexcept {
    return static_cast<std::size_t>(tile.y) * local::kLocalWidth + tile.x;
}

} // namespace

namespace detail {

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

} // namespace detail

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

} // namespace aetheria::runtime
