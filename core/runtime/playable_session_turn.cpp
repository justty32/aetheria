// playable_session_turn.cpp：移動與回合推進。

#include "core/runtime/playable_session.h"
#include "core/runtime/save_raws.h"
#include "core/runtime/turn_commit.h"

#include "core/site/site_reduction.h"
#include "core/world/faction_ai.h"


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

namespace {

[[nodiscard]] std::string move_payload(world::StableId unit,
                                       world::RegionXY target) {
    return "unit = " + std::to_string(unit.uid) + "\ntarget_x = " +
           std::to_string(target.x) + "\ntarget_y = " +
           std::to_string(target.y) + "\n";
}

[[nodiscard]] std::uint8_t action_value(ai::FactionActionKind action) noexcept {
    return static_cast<std::uint8_t>(action);
}

} // namespace

void PlayableSession::issue_move(world::StableId unit,
                                 world::RegionXY target) {
    record_command("issue_move", move_payload(unit, target));
    if (encounter_tile_) {
        throw std::logic_error{"遭遇尚未處理，不能發布移動命令"};
    }
    if (!army(unit).player_controlled) {
        throw std::invalid_argument{"玩家不能替 NPC 部隊發布移動命令"};
    }
    turn_pipeline_.issue_move(*region_, unit, target);
    ++revision_;
    append_event(PlayableEventKind::MoveIssued, target,
                 static_cast<std::int64_t>(unit.uid));
}

void PlayableSession::detect_encounter() {
    const auto player = position_of(player_army_id_).tile;
    const auto enemy = position_of(enemy_army_id_).tile;
    const auto distance = std::abs(static_cast<std::int32_t>(player.x) - enemy.x) +
                          std::abs(static_cast<std::int32_t>(player.y) - enemy.y);
    if (distance > 0) {
        return;
    }
    encounter_tile_ = player;
    region_->reg.clear<world::RegionMoveCommand>();
    append_event(PlayableEventKind::Encounter, player,
                 army(player_army_id_).power, army(enemy_army_id_).power);
}

PlayableAdvanceReport PlayableSession::advance_xun() {
    record_command("advance_xun");
    if (encounter_tile_) {
        throw std::logic_error{"遭遇尚未處理，不能推進下一旬"};
    }
    PlayableAdvanceReport report;
    const auto player_before = position_of(player_army_id_).tile;
    const auto enemy_before = position_of(enemy_army_id_).tile;
    turn_pipeline_.advance_xun(
        *region_,
        [&](world::TurnStage stage) {
            report.stages.push_back(stage);
            if (stage == world::TurnStage::Encounters) {
                detect_encounter();
            }
        },
        [&](zone::Zone& reducing_region) {
            auto& reducing_tiles =
                std::get<zone::RegionPayload>(reducing_region.payload).layers.at(0);
            site::reduce_live_site_xun(reducing_tiles, battle_tile_, *battle_site_, ruleset_);
        },
        [&](time::Tick tick) {
            for (const auto faction : {world::FactionId{2}, world::FactionId{3}}) {
                const auto ai_report = world::advance_faction_ai_xun(
                    *diplomacy_, faction, {4, 0, true, true, faction == world::FactionId{2}},
                    tick, ruleset_);
                const auto action = action_value(ai_report.decision.command.kind);
                report.ai_actions.push_back(action);
                append_event(PlayableEventKind::FactionAiActed, {},
                             static_cast<std::int64_t>(static_cast<std::uint16_t>(faction)),
                             action);
            }
        });
    if (!replaying_history_ && !encounter_tile_.has_value() &&
        turn_commit_->history().bound()) {
        if (!character_save_path_.has_value()) {
            throw std::logic_error{"已綁定世界槽但缺少目前角色檔路徑"};
        }
        const auto raws_hash = raws_directory_hash(base_raws_directory_);
        turn_commit_->commit_world(store_, store_, *manager_, seed_,
                                   raws_hash, now(), [&] {
                                       write_character_save(
                                           *character_save_path_,
                                           {seed_, raws_hash},
                                           export_character_state());
                                   });
    }
    const auto player_after = position_of(player_army_id_).tile;
    const auto enemy_after = position_of(enemy_army_id_).tile;
    append_event(PlayableEventKind::XunAdvanced, player_after,
                 static_cast<std::int64_t>(now()));
    if (enemy_after != enemy_before) {
        append_event(PlayableEventKind::EnemyMoved, enemy_after, enemy_before.x,
                     enemy_after.x);
    }
    if (player_after == player_before && enemy_after == enemy_before &&
        !encounter_tile_) {
        append_event(PlayableEventKind::EnemyMoved, enemy_after, enemy_before.x,
                     enemy_after.x);
    }
    ++revision_;
    report.encounter_pending = encounter_tile_.has_value();
    return report;
}

} // namespace aetheria::runtime
