#include "core/runtime/turn_commit.h"

#include "core/runtime/playable_session.h"
#include "core/runtime/session_persistence.h"
#include "core/zone/zone_manager.h"
#include "core/zone/zone_store.h"

#include <array>
#include <charconv>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

#include "core/runtime/turn_commit_detail.h"

namespace aetheria::runtime {
using namespace detail;

void TurnCommit::replay_tail(PlayableSession& session) const {
    history_.replay_after(committed_seq_,
                          [&](const auto& entry) { apply_entry(session, entry); });
}

void TurnCommit::replay_all(PlayableSession& session) const {
    history_.replay_after(0U,
                          [&](const auto& entry) { apply_entry(session, entry); });
}

void TurnCommit::apply_entry(PlayableSession& session,
                             const history::HistoryEntry& entry) const {
    if (entry.payload.find("context_residence = ") != std::string::npos) {
        const auto residence = parse_string(entry.payload, "context_residence");
        if (residence == "region") {
            session.residence_ = PlayableResidence::Region;
        } else if (residence == "site") {
            session.residence_ = PlayableResidence::Site;
        } else if (residence == "local") {
            session.residence_ = PlayableResidence::Local;
        } else if (residence == "dungeon") {
            session.residence_ = PlayableResidence::Dungeon;
        } else {
            throw std::runtime_error{"history context residence 無效：" + residence};
        }
        const auto local_z = parse_i64(entry.payload, "context_local_z");
        const auto player_x = parse_u64(entry.payload, "context_player_x");
        const auto player_y = parse_u64(entry.payload, "context_player_y");
        const auto army = parse_u64(entry.payload, "context_player_army");
        const auto has_accepted =
            parse_u64(entry.payload, "context_has_accepted_quest");
        const auto accepted = parse_u64(entry.payload, "context_accepted_quest");
        if (local_z < std::numeric_limits<std::int8_t>::min() ||
            local_z > std::numeric_limits<std::int8_t>::max() ||
            player_x > std::numeric_limits<std::uint16_t>::max() ||
            player_y > std::numeric_limits<std::uint16_t>::max() ||
            has_accepted > 1U) {
            throw std::runtime_error{"history character context 超出範圍"};
        }
        session.local_z_ = static_cast<std::int8_t>(local_z);
        session.local_player_x_ = static_cast<std::uint16_t>(player_x);
        session.local_player_y_ = static_cast<std::uint16_t>(player_y);
        session.player_army_id_ = world::StableId{army};
        session.accepted_quest_id_ = has_accepted == 0U
                                         ? std::nullopt
                                         : std::optional<std::uint64_t>{accepted};
    }
    session.replaying_history_ = true;
    session.current_command_seq_ = entry.seq;
    try {
        if (entry.kind == "issue_move") {
            const auto unit = parse_u64(entry.payload, "unit");
            const auto x = parse_i64(entry.payload, "target_x");
            const auto y = parse_i64(entry.payload, "target_y");
            if (x < std::numeric_limits<std::int16_t>::min() ||
                x > std::numeric_limits<std::int16_t>::max() ||
                y < std::numeric_limits<std::int16_t>::min() ||
                y > std::numeric_limits<std::int16_t>::max()) {
                throw std::runtime_error{"history issue_move 座標超出 int16"};
            }
            session.issue_move(world::StableId{unit},
                               {static_cast<std::int16_t>(x),
                                static_cast<std::int16_t>(y)});
        } else if (entry.kind == "advance_xun") {
            static_cast<void>(session.advance_xun());
        } else if (entry.kind == "resolve_encounter") {
            const auto choice = parse_string(entry.payload, "choice");
            static_cast<void>(session.resolve_encounter(
                choice == "manual" ? PlayableBattleChoice::CommandSite
                                     : PlayableBattleChoice::AutoRegion));
        } else if (entry.kind == "enter_site_manual") {
            session.enter_site();
        } else if (entry.kind == "enter_site_auto") {
            session.manage_city();
        } else if (entry.kind == "leave_site") {
            session.leave_site();
        } else if (entry.kind == "build_city") {
            session.build_city();
        } else if (entry.kind == "accept_bandit") {
            session.accept_bandit_quest();
        } else if (entry.kind == "enter_local_manual") {
            session.enter_local();
        } else if (entry.kind == "enter_local_auto") {
            session.manage_local();
        } else if (entry.kind == "leave_local") {
            session.leave_local();
        } else if (entry.kind == "open_door") {
            session.open_door_and_move();
        } else if (entry.kind == "suppress_bandits") {
            session.suppress_bandits();
        } else if (entry.kind == "enter_dungeon_manual") {
            session.enter_dungeon();
        } else if (entry.kind == "enter_dungeon_auto") {
            session.manage_dungeon();
        } else if (entry.kind == "leave_dungeon") {
            session.leave_dungeon();
        } else if (entry.kind == "descend_dungeon") {
            session.descend_dungeon();
        } else if (entry.kind == "clear_dungeon") {
            session.clear_dungeon();
        } else if (entry.kind == "sign_treaty") {
            session.sign_peace_treaty();
        } else if (entry.kind == "measure_roundtrips") {
            session.measure_site_roundtrips();
        } else if (entry.kind == "measure_calibration") {
            session.measure_city_management(
                static_cast<std::uint32_t>(parse_u64(entry.payload, "samples")));
        } else {
            throw std::runtime_error{"history kind 不支援重放：" + entry.kind};
        }
    } catch (...) {
        session.replaying_history_ = false;
        throw;
    }
    session.replaying_history_ = false;
}

} // namespace aetheria::runtime
