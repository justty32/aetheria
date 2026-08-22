#pragma once

// playable_session.h：把既有 Region 移動、勢力 AI、三層戰鬥與歸約接成最小可玩情境。
// 所有玩法狀態由此純 C++ session 擁有；bridge 與 Godot 只取得批次快照。

#include "core/rules/combat.h"
#include "core/rules/ruleset.h"
#include "core/time/tick.h"
#include "core/world/combat_scaling.h"
#include "core/world/diplomacy.h"
#include "core/world/named_fate.h"
#include "core/world/region_movement.h"
#include "core/world/region_tiles.h"
#include "core/zone/zone_store.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aetheria::runtime {

enum class PlayableBattleChoice : std::uint8_t { CommandSite, AutoRegion };

enum class PlayableEventKind : std::uint8_t {
    NewGame,
    MoveIssued,
    XunAdvanced,
    FactionAiActed,
    EnemyMoved,
    Encounter,
    BattleResolved,
    WorldChanged,
};

struct PlayableEvent {
    std::uint64_t id{};
    PlayableEventKind kind{PlayableEventKind::NewGame};
    world::RegionXY tile;
    std::int64_t value_a{};
    std::int64_t value_b{};
};

struct PlayableArmy {
    world::StableId id;
    world::FactionId faction{};
    std::int32_t power{};
    bool player_controlled{};
};

struct PlayableArmyView {
    world::StableId id;
    world::FactionId faction{};
    world::RegionXY tile;
    std::int32_t power{};
    bool player_controlled{};
};

struct PlayableTileState {
    world::FactionId owner{};
    world::PopulationReduction::Value population{};
    world::OrderReduction::Value order{};
};

struct PlayableBattleReport {
    PlayableBattleChoice choice{PlayableBattleChoice::AutoRegion};
    rules::CombatResult region_result;
    world::LayerCombatResult layer_result;
    world::RegionXY tile;
    PlayableTileState before;
    PlayableTileState after;
    world::FateOutcome named_outcome{world::FateOutcome::Unharmed};
    std::string named_person;
};

struct PlayableAdvanceReport {
    std::vector<world::TurnStage> stages;
    std::vector<std::uint8_t> ai_actions;
    bool encounter_pending{};
};

// PlayableSession 是 M8.1 的 core 編排門面。呼叫端獨占 session；所有 getter
// 回傳的參考在下一個 mutating command 或 session 析構前有效。
class PlayableSession {
public:
    PlayableSession(std::uint64_t seed, std::uint32_t region_id,
                    std::string data_directory);

    PlayableSession(const PlayableSession&) = delete;
    PlayableSession& operator=(const PlayableSession&) = delete;

    void issue_move(world::StableId unit, world::RegionXY target);
    [[nodiscard]] PlayableAdvanceReport advance_xun();
    [[nodiscard]] const PlayableBattleReport&
    resolve_encounter(PlayableBattleChoice choice);

    [[nodiscard]] const world::RegionTiles& tiles() const;
    [[nodiscard]] const rules::Ruleset& ruleset() const noexcept { return ruleset_; }
    [[nodiscard]] std::vector<PlayableArmyView> armies() const;
    [[nodiscard]] PlayableTileState tile_state(world::RegionXY tile) const;
    [[nodiscard]] time::Tick now() const;
    [[nodiscard]] bool encounter_pending() const noexcept {
        return encounter_tile_.has_value();
    }
    [[nodiscard]] const std::optional<PlayableBattleReport>& battle_report() const noexcept {
        return battle_report_;
    }
    [[nodiscard]] const std::vector<PlayableEvent>& events() const noexcept {
        return events_;
    }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] world::StableId player_army_id() const noexcept {
        return player_army_id_;
    }
    [[nodiscard]] world::RegionXY guided_target() const noexcept {
        return enemy_start_;
    }
    [[nodiscard]] world::RegionXY battle_tile() const noexcept {
        return battle_tile_;
    }

private:
    [[nodiscard]] world::RegionPosition& position_of(world::StableId unit);
    [[nodiscard]] const world::RegionPosition& position_of(world::StableId unit) const;
    [[nodiscard]] PlayableArmy& army(world::StableId unit);
    [[nodiscard]] const PlayableArmy& army(world::StableId unit) const;
    void append_event(PlayableEventKind kind, world::RegionXY tile = {},
                      std::int64_t value_a = 0, std::int64_t value_b = 0);
    void detect_encounter();
    void initialize_scenario();
    void initialize_diplomacy();

    std::uint64_t seed_{};
    std::uint32_t region_id_{};
    rules::Ruleset ruleset_;
    zone::InMemoryZoneStore store_;
    world::RegionTurnPipeline turn_pipeline_;
    world::WorldDiplomacyState diplomacy_;
    std::unique_ptr<zone::Zone> region_;
    std::optional<zone::Zone> battle_site_;
    std::vector<PlayableArmy> armies_;
    std::optional<world::RegionXY> encounter_tile_;
    std::optional<PlayableBattleReport> battle_report_;
    std::vector<PlayableEvent> events_;
    std::uint64_t next_event_id_{1};
    std::uint64_t revision_{1};
    world::StableId player_army_id_{1001};
    world::StableId enemy_army_id_{2001};
    world::RegionXY player_start_{61, 48};
    world::RegionXY enemy_start_{65, 48};
    world::RegionXY battle_tile_{63, 48};
};

} // namespace aetheria::runtime
