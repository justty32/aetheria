// playable_session_character.cpp：角色狀態存取。

#include "core/runtime/playable_session.h"

#include "core/site/site_materialize.h"


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

namespace {

[[nodiscard]] PlayableResidence parse_residence(std::string_view id) {
    if (id == "region") {
        return PlayableResidence::Region;
    }
    if (id == "site") {
        return PlayableResidence::Site;
    }
    if (id == "local") {
        return PlayableResidence::Local;
    }
    if (id == "dungeon") {
        return PlayableResidence::Dungeon;
    }
    throw std::runtime_error{"角色檔 residence_id 不支援：" + std::string{id}};
}

} // namespace

namespace detail {

[[nodiscard]] std::string_view residence_id(PlayableResidence residence) {
    switch (residence) {
    case PlayableResidence::Region:
        return "region";
    case PlayableResidence::Site:
        return "site";
    case PlayableResidence::Local:
        return "local";
    case PlayableResidence::Dungeon:
        return "dungeon";
    }
    throw std::logic_error{"未知的角色駐留層"};
}

} // namespace detail

CharacterState PlayableSession::export_character_state() const {
    return {player_army_id_, std::string{residence_id(residence_)}, local_z_,
            local_player_x_, local_player_y_, accepted_quest_id_};
}

void PlayableSession::import_character_state(const CharacterState& state) {
    const auto residence = parse_residence(state.residence_id);
    if (state.local_player_x >= local::kLocalWidth ||
        state.local_player_y >= local::kLocalHeight) {
        throw std::runtime_error{"角色檔 Local 座標超出 64x64 範圍"};
    }
    if ((residence == PlayableResidence::Dungeon && state.local_z >= 0) ||
        (residence != PlayableResidence::Dungeon && state.local_z != 0)) {
        throw std::runtime_error{"角色檔 residence_id 與 local_z 不一致"};
    }
    const auto army_handle = std::ranges::find(armies_, state.player_army_id,
                                               &PlayableArmy::id);
    if (army_handle == armies_.end() ||
        !army(state.player_army_id).player_controlled) {
        throw std::runtime_error{"角色檔 player_army_id 在世界 registry 中不存在或非玩家部隊：" +
                                 std::to_string(state.player_army_id.uid)};
    }
    if (state.accepted_quest_id.has_value() &&
        std::ranges::none_of(quests_, [&](const auto& quest) {
            return quest.id == *state.accepted_quest_id;
        })) {
        throw std::runtime_error{"角色檔 accepted_quest_id 已無法由世界態重建：" +
                                 std::to_string(*state.accepted_quest_id)};
    }

    if (residence != PlayableResidence::Region) {
        const auto site_handle = acquire_coverage_site();
        auto& region_tiles =
            std::get<zone::RegionPayload>(region_->payload).layers.at(0);
        const bool entered = manager_->with(site_handle, [&](zone::Zone& loaded) {
            site::enter_full_site(loaded, region_tiles, coverage_tile_);
        });
        if (!entered) {
            throw std::runtime_error{"角色駐留層還原時無法進入 Site"};
        }
        if (residence == PlayableResidence::Local ||
            residence == PlayableResidence::Dungeon) {
            const auto local_handle = acquire_coverage_local();
            if (residence == PlayableResidence::Dungeon) {
                bool floor_exists{};
                static_cast<void>(manager_->with(
                    local_handle, [&](const zone::Zone& loaded) {
                        floor_exists = std::get<zone::LocalPayload>(loaded.payload)
                                           .layers.contains(state.local_z);
                    }));
                if (!floor_exists) {
                    throw std::runtime_error{"角色檔 local_z 對應的地城層不存在"};
                }
            }
        }
    }

    player_army_id_ = state.player_army_id;
    residence_ = residence;
    local_z_ = state.local_z;
    local_player_x_ = state.local_player_x;
    local_player_y_ = state.local_player_y;
    accepted_quest_id_ = state.accepted_quest_id;
}

} // namespace aetheria::runtime
