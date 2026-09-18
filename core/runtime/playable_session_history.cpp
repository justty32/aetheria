// playable_session_history.cpp：事件與歷史日誌。

#include "core/runtime/playable_session.h"
#include "core/runtime/save_raws.h"
#include "core/runtime/turn_commit.h"



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

void PlayableSession::append_event(PlayableEventKind kind, world::RegionXY tile,
                                   std::int64_t value_a,
                                   std::int64_t value_b) {
    events_.push_back({next_event_id_++, kind, tile, value_a, value_b});
}

void PlayableSession::record_command(std::string_view kind, std::string payload) {
    if (replaying_history_) {
        return;
    }
    std::string context = "context_residence = \"" +
                          std::string{residence_id(residence_)} +
                          "\"\ncontext_local_z = " + std::to_string(local_z_) +
                          "\ncontext_player_x = " +
                          std::to_string(local_player_x_) +
                          "\ncontext_player_y = " +
                          std::to_string(local_player_y_) +
                          "\ncontext_player_army = " +
                          std::to_string(player_army_id_.uid) +
                          "\ncontext_has_accepted_quest = " +
                          (accepted_quest_id_.has_value() ? std::string{"1"}
                                                          : std::string{"0"}) +
                          "\ncontext_accepted_quest = " +
                          std::to_string(accepted_quest_id_.value_or(0U)) +
                          "\n";
    context += payload;
    current_command_seq_ = turn_commit_->record(now(), kind, context).seq;
}

void PlayableSession::replay_history_from(
    const std::filesystem::path& slot_directory) {
    const auto source_raws_hash = save_raws_hash(slot_directory);
    if (source_raws_hash != raws_directory_hash(base_raws_directory_)) {
        throw std::runtime_error{"replay 的基底 raws 與 session 不同"};
    }
    TurnCommit source{slot_directory};
    source.replay_all(*this);
}

std::uint64_t PlayableSession::history_head_hash() const noexcept {
    return turn_commit_->history().head_hash();
}

std::uint64_t PlayableSession::history_head_seq() const noexcept {
    return turn_commit_->history().head_seq();
}

void PlayableSession::set_interrupt_after_journal_for_testing(bool enabled) {
    turn_commit_->set_interrupt_after_journal_for_testing(enabled);
}

void PlayableSession::set_interrupt_after_save_for_testing(bool enabled) {
    turn_commit_->set_interrupt_after_save_for_testing(enabled);
}

} // namespace aetheria::runtime
