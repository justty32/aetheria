// playable_session_load.cpp：載入狀態重建。

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


namespace aetheria::runtime {

void PlayableSession::bind_loaded_zone(zone::ZoneHandle handle,
                                       zone::Zone*& destination,
                                       std::string_view description) {
    const bool borrowed = manager_->with(handle, [&](zone::Zone& loaded) {
        loaded.pinned = true;
        destination = &loaded;
    });
    if (!borrowed || destination == nullptr) {
        throw std::runtime_error{"無法綁定已載入 zone：" + std::string{description}};
    }
}

void PlayableSession::rebuild_army_handles() {
    armies_.clear();
    bool found_player{};
    std::optional<world::StableId> found_enemy;
    for (const auto entity :
         region_->reg.view<const world::StableId, const world::ArmyState,
                           const world::RegionPosition>()) {
        const auto id = region_->reg.get<const world::StableId>(entity);
        const auto& state = region_->reg.get<const world::ArmyState>(entity);
        store_.observe_entity_uid(id.uid);
        armies_.push_back({id, entity});
        if (state.player_controlled) {
            if (found_player) {
                throw std::runtime_error{"存檔含多個玩家控制部隊"};
            }
            player_army_id_ = id;
            found_player = true;
        } else {
            if (found_enemy.has_value()) {
                throw std::runtime_error{"存檔含多個非玩家可玩情境部隊"};
            }
            found_enemy = id;
        }
    }
    if (found_enemy.has_value()) {
        enemy_army_id_ = *found_enemy;
    }
    std::ranges::sort(armies_, {}, [](const PlayableArmy& army) {
        return army.id.uid;
    });
    if (armies_.empty() || !found_player ||
        std::ranges::none_of(armies_, [&](const auto& army_handle) {
            return army_handle.id == player_army_id_;
        }) ||
        std::ranges::none_of(armies_, [&](const auto& army_handle) {
            return army_handle.id == enemy_army_id_;
        })) {
        throw std::runtime_error{"存檔缺少可玩情境所需的玩家或敵方部隊"};
    }
}

void PlayableSession::initialize_loaded_scenario() {
    battle_site_key_ = zone::child_key(region_key_, battle_tile_.x, battle_tile_.y);
    coverage_site_key_ =
        zone::child_key(region_key_, coverage_tile_.x, coverage_tile_.y);
    coverage_local_key_ = zone::child_key(coverage_site_key_, local_entrance_.x,
                                          local_entrance_.y);
    for (const auto [key, description] :
         std::array{std::pair{region_key_, std::string_view{"Region"}},
                    std::pair{battle_site_key_, std::string_view{"戰鬥 Site"}},
                    std::pair{coverage_site_key_, std::string_view{"覆蓋 Site"}}}) {
        if (!store_.contains(key)) {
            throw std::runtime_error{"存檔缺少必要 zone：" + std::string{description}};
        }
    }

    bind_loaded_zone(manager_->require(region_key_), region_, "Region");
    const auto battle = manager_->acquire(battle_site_key_);
    if (!battle.has_value()) {
        throw std::runtime_error{"無法從存檔重展開戰鬥 Site"};
    }
    bind_loaded_zone(*battle, battle_site_, "戰鬥 Site");

    const auto root = manager_->get(zone::kRootZone);
    const bool root_bound = root.has_value() && manager_->with(*root, [&](zone::Zone& loaded) {
        if (loaded.diplomacy.has_value()) {
            diplomacy_ = &*loaded.diplomacy;
        }
    });
    if (!root_bound || diplomacy_ == nullptr) {
        throw std::runtime_error{"存檔 root 缺少外交狀態"};
    }
    rebuild_army_handles();

    const auto coverage = manager_->acquire(coverage_site_key_);
    if (!coverage.has_value()) {
        throw std::runtime_error{"無法從存檔重展開覆蓋 Site"};
    }
    const bool refreshed = manager_->with(*coverage, [&](zone::Zone& loaded) {
        refresh_quests(loaded);
    });
    if (!refreshed) {
        throw std::runtime_error{"冷讀後無法重算湧現任務"};
    }
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    site::unload_site_zone(*manager_, *coverage, region_tiles, coverage_tile_, seed_,
                           region_id_, now(), ruleset_);
    append_event(PlayableEventKind::NewGame, player_start_, region_id_, seed_);
}

} // namespace aetheria::runtime
