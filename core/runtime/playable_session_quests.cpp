// playable_session_quests.cpp：清剿任務。

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

void PlayableSession::refresh_quests(zone::Zone& site_zone) {
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    const std::array<const zone::Zone*, 1> loaded_sites{&site_zone};
    quests_ = narrative::detect_emergent_quests(
        {.region_tiles = &region_tiles,
         .loaded_sites = loaded_sites,
         .faction_tensions = {}},
        ruleset_);
    if (accepted_quest_id_.has_value() &&
        std::ranges::none_of(quests_, [&](const auto& quest) {
            return quest.id == *accepted_quest_id_;
        })) {
        accepted_quest_id_.reset();
    }
}

void PlayableSession::accept_bandit_quest() {
    record_command("accept_bandit");
    if (residence_ != PlayableResidence::Site) {
        throw std::logic_error{"必須在 Site 接受清剿任務"};
    }
    const auto found = std::ranges::find(quests_, narrative::EmergentQuestKind::BanditSuppression,
                                         &narrative::EmergentQuest::kind);
    if (found == quests_.end()) {
        throw std::logic_error{"目前沒有清剿盜匪需求"};
    }
    accepted_quest_id_ = found->id;
    ++revision_;
    append_event(PlayableEventKind::QuestAccepted, coverage_tile_,
                 static_cast<std::int64_t>(found->id));
}

void PlayableSession::perform_bandit_suppression(bool managed) {
    if ((!managed && residence_ != PlayableResidence::Local) ||
        (managed && residence_ != PlayableResidence::Site)) {
        throw std::logic_error{"清剿命令的駐留層不符"};
    }
    const auto quest = accepted_quest_id_.has_value()
                           ? std::ranges::find(quests_, *accepted_quest_id_,
                                               &narrative::EmergentQuest::id)
                           : quests_.end();
    if (quest == quests_.end() ||
        quest->kind != narrative::EmergentQuestKind::BanditSuppression) {
        throw std::logic_error{"必須先接受仍存在的清剿任務"};
    }
    const auto site_handle = acquire_coverage_site();
    const auto local_handle = acquire_coverage_local();
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    const std::array handles{site_handle, local_handle};
    const bool borrowed = manager_->with_many(
        handles, [&](std::span<zone::Zone* const> zones) {
            const auto report = narrative::complete_bandit_suppression(
                *quest, region_tiles, *zones[0], ruleset_);
            last_order_before_ = report.order_before;
            last_order_after_ = report.order_after;
            refresh_quests(*zones[0]);
        });
    if (!borrowed) {
        throw std::logic_error{"清剿時 Site/Local 未同時載入"};
    }
    if (managed) {
        if (!manager_->unload(coverage_local_key_)) {
            throw std::logic_error{"代管清剿後 Local 卸載失敗"};
        }
        append_event(PlayableEventKind::ManagedActivity, coverage_tile_,
                     last_order_before_, last_order_after_);
    }
    ++revision_;
    append_event(PlayableEventKind::QuestCompleted, coverage_tile_,
                 last_order_before_, last_order_after_);
}

void PlayableSession::suppress_bandits() {
    record_command("suppress_bandits");
    perform_bandit_suppression(false);
}

void PlayableSession::manage_local() {
    record_command("enter_local_auto");
    perform_bandit_suppression(true);
}

} // namespace aetheria::runtime
