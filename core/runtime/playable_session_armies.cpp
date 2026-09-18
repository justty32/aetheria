// playable_session_armies.cpp：部隊與 Region 查詢。

#include "core/runtime/playable_session.h"



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


namespace aetheria::runtime {

world::RegionPosition& PlayableSession::position_of(world::StableId unit) {
    for (const auto entity :
         region_->reg.view<const world::StableId, world::RegionPosition>()) {
        if (region_->reg.get<const world::StableId>(entity) == unit) {
            return region_->reg.get<world::RegionPosition>(entity);
        }
    }
    throw std::invalid_argument{"找不到指定的 Region 部隊"};
}

const world::RegionPosition&
PlayableSession::position_of(world::StableId unit) const {
    for (const auto entity :
         region_->reg.view<const world::StableId, const world::RegionPosition>()) {
        if (region_->reg.get<const world::StableId>(entity) == unit) {
            return region_->reg.get<const world::RegionPosition>(entity);
        }
    }
    throw std::invalid_argument{"找不到指定的 Region 部隊"};
}

world::ArmyState& PlayableSession::army(world::StableId unit) {
    const auto found =
        std::ranges::find(armies_, unit, &PlayableArmy::id);
    if (found == armies_.end()) {
        throw std::invalid_argument{"找不到指定的部隊資料"};
    }
    return region_->reg.get<world::ArmyState>(found->entity);
}

const world::ArmyState& PlayableSession::army(world::StableId unit) const {
    const auto found =
        std::ranges::find(armies_, unit, &PlayableArmy::id);
    if (found == armies_.end()) {
        throw std::invalid_argument{"找不到指定的部隊資料"};
    }
    return region_->reg.get<const world::ArmyState>(found->entity);
}

const world::RegionTiles& PlayableSession::tiles() const {
    return std::get<zone::RegionPayload>(region_->payload).layers.at(0);
}

std::vector<PlayableArmyView> PlayableSession::armies() const {
    std::vector<PlayableArmyView> result;
    result.reserve(armies_.size());
    for (const auto& value : armies_) {
        const auto& state = region_->reg.get<const world::ArmyState>(value.entity);
        result.push_back({value.id, state.faction, position_of(value.id).tile,
                          state.power, state.player_controlled});
    }
    return result;
}

PlayableTileState PlayableSession::tile_state(world::RegionXY tile) const {
    const auto& region_tiles = tiles();
    const auto index = region_tiles.index_of(tile);
    return {region_tiles.owner[index],
            region_tiles.reduction_value<world::PopulationReduction>(tile),
            region_tiles.reduction_value<world::OrderReduction>(tile)};
}

time::Tick PlayableSession::now() const {
    return world::turn_clock(*region_).now;
}

} // namespace aetheria::runtime
