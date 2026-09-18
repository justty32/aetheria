// playable_session_dungeon.cpp：地城操作。

#include "core/runtime/playable_session.h"

#include "core/local/dungeon.h"


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

void PlayableSession::enter_dungeon() {
    record_command("enter_dungeon_manual");
    if (residence_ != PlayableResidence::Local) {
        throw std::logic_error{"只有 Local 駐留可親自下地城"};
    }
    bool has_floor{};
    const auto handle = manager_->get(coverage_local_key_);
    if (handle.has_value()) {
        static_cast<void>(manager_->with(*handle, [&](const zone::Zone& loaded) {
            has_floor = std::get<zone::LocalPayload>(loaded.payload).layers.contains(-1);
        }));
    }
    if (!has_floor) {
        throw std::logic_error{"Local 找不到負 z 地城入口"};
    }
    residence_ = PlayableResidence::Dungeon;
    local_z_ = -1;
    ++revision_;
    append_event(PlayableEventKind::DungeonEntered, coverage_tile_, local_z_);
}

void PlayableSession::leave_dungeon() {
    record_command("leave_dungeon");
    if (residence_ != PlayableResidence::Dungeon) {
        throw std::logic_error{"目前不在地城"};
    }
    residence_ = PlayableResidence::Local;
    local_z_ = 0;
    ++revision_;
}

void PlayableSession::descend_dungeon() {
    record_command("descend_dungeon");
    if (residence_ != PlayableResidence::Dungeon) {
        throw std::logic_error{"只有地城駐留可往下層"};
    }
    const auto next = static_cast<std::int8_t>(local_z_ - 1);
    bool exists{};
    const auto handle = manager_->get(coverage_local_key_);
    if (handle.has_value()) {
        static_cast<void>(manager_->with(*handle, [&](const zone::Zone& loaded) {
            exists = std::get<zone::LocalPayload>(loaded.payload).layers.contains(next);
        }));
    }
    if (!exists) {
        throw std::logic_error{"已在地城最深層"};
    }
    local_z_ = next;
    ++revision_;
    append_event(PlayableEventKind::DungeonEntered, coverage_tile_, local_z_);
}

void PlayableSession::perform_dungeon_clear(bool managed) {
    if ((!managed && residence_ != PlayableResidence::Dungeon) ||
        (managed && residence_ != PlayableResidence::Local)) {
        throw std::logic_error{"地城清理命令的駐留層不符"};
    }
    const auto site_handle = acquire_coverage_site();
    const auto local_handle = acquire_coverage_local();
    const std::array handles{site_handle, local_handle};
    const bool borrowed = manager_->with_many(
        handles, [&](std::span<zone::Zone* const> zones) {
            auto& site_persistent =
                std::get<zone::SitePayload>(zones[0]->payload).layers.persistent;
            if (site_persistent.dungeons.empty() ||
                site_persistent.dungeons.front().cleared) {
                throw std::logic_error{"地城已經清空"};
            }
            auto& local_persistent =
                std::get<zone::LocalPayload>(zones[1]->payload).dungeon;
            auto dungeon = local::generate_dungeon(
                seed_ ^ UINT64_C(0xD006E002), site_persistent.dungeons.front(),
                local_persistent, ruleset_);
            dungeon_density_before_ = static_cast<std::uint16_t>(std::accumulate(
                dungeon.floors.begin(), dungeon.floors.end(), 0U,
                [](std::uint32_t total, const auto& floor) {
                    return total + floor.enemy_count;
                }));
            if (!dungeon.floors.empty() && !dungeon.floors.front().traps.empty()) {
                static_cast<void>(local::trigger_trap(
                    dungeon.floors.front().traps.front(), player_army_id_.uid, false,
                    local_persistent, ruleset_));
            }
            local::claim_all_treasure(dungeon, local_persistent);
            static_cast<void>(local::defeat_boss(dungeon.boss, dungeon.uid));
            site_persistent.dungeons.front().cleared = true;
            const auto reduced = local::generate_dungeon(
                seed_ ^ UINT64_C(0xD006E002), site_persistent.dungeons.front(),
                local_persistent, ruleset_);
            dungeon_density_after_ = static_cast<std::uint16_t>(std::accumulate(
                reduced.floors.begin(), reduced.floors.end(), 0U,
                [](std::uint32_t total, const auto& floor) {
                    return total + floor.enemy_count;
                }));
            refresh_quests(*zones[0]);
        });
    if (!borrowed) {
        throw std::logic_error{"清地城時 Site/Local 未同時載入"};
    }
    ++revision_;
    append_event(PlayableEventKind::DungeonCleared, coverage_tile_,
                 dungeon_density_before_, dungeon_density_after_);
    if (managed) {
        append_event(PlayableEventKind::ManagedActivity, coverage_tile_,
                     dungeon_density_before_, dungeon_density_after_);
    }
}

void PlayableSession::clear_dungeon() {
    record_command("clear_dungeon");
    perform_dungeon_clear(false);
}

void PlayableSession::manage_dungeon() {
    record_command("enter_dungeon_auto");
    perform_dungeon_clear(true);
}

} // namespace aetheria::runtime
