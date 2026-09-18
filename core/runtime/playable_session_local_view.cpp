// playable_session_local_view.cpp：Local 批次快照。

#include "core/runtime/playable_session.h"

#include "core/local/local_fov.h"

#include <aetheria/runtime/cross_zone.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>


#include "core/runtime/playable_session_detail.h"

namespace aetheria::runtime {
using namespace detail;

std::optional<PlayableGridView> PlayableSession::local_view() const {
    const auto handle = manager_->get(coverage_local_key_);
    if (!handle.has_value()) {
        return std::nullopt;
    }
    PlayableGridView result{local::kLocalWidth, local::kLocalHeight, local_z_,
                            std::vector<std::uint8_t>(local::kLocalTileCount),
                            local_player_x_, local_player_y_};
    std::vector<std::uint8_t> visible(local::kLocalTileCount,
                                      local_z_ == 0 ? UINT8_C(0) : UINT8_C(1));
    local::DoorStateQuery doors;
    const bool has_door_state =
        manager_->with(*handle, [&](const zone::Zone& loaded) {
            doors = door_query(loaded);
        });
    if (!has_door_state) {
        return std::nullopt;
    }
    if (local_z_ == 0) {
        runtime::CrossZoneRuntime runtime{*manager_};
        const auto fov = local::calculate_fov(
            runtime, ruleset_,
            {coverage_local_key_, {local_player_x_, local_player_y_}}, {12, 12},
            doors);
        for (const auto& location : fov.visible) {
            if (location.zone == coverage_local_key_) {
                visible[local_index(location.tile)] = UINT8_C(1);
            }
        }
    }
    const bool borrowed = manager_->with(*handle, [&](const zone::Zone& loaded) {
        const auto& payload = std::get<zone::LocalPayload>(loaded.payload);
        const auto found = payload.layers.find(local_z_);
        if (found == payload.layers.end()) {
            return;
        }
        for (std::size_t index = 0; index < result.cells.size(); ++index) {
            if (visible[index] == 0) {
                result.cells[index] = UINT8_C(0);
            } else if (found->second.overlay[index] == local::OverlayId::Stairs) {
                result.cells[index] = UINT8_C(4);
            } else if (found->second.light[index] != 0) {
                result.cells[index] = UINT8_C(2);
            } else {
                result.cells[index] = UINT8_C(1);
            }
        }
    });
    if (!borrowed) {
        return std::nullopt;
    }
    if (local_z_ == 0) {
        result.cells[local_index({local_player_x_, local_player_y_})] = UINT8_C(5);
        const auto door = local::local_edge_address(
            {coverage_local_key_, {31, 32}}, spatial::BoundarySide::East);
        if (!door.has_value() ||
            local::query_door_state(doors, *door) != local::DoorState::Open) {
            result.cells[local_index({32, 32})] = UINT8_C(6);
        }
    }
    return result;
}

} // namespace aetheria::runtime
