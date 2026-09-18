#pragma once

// playable_session.h：把既有 Region 移動、勢力 AI、三層戰鬥與歸約接成最小可玩情境。
// 所有玩法狀態由此純 C++ session 擁有；bridge 與 Godot 只取得批次快照。

#include "core/runtime/playable_session_views.h"

namespace aetheria::zone {
class FileZoneStore;
}

namespace aetheria::runtime {

class TurnCommit;

enum class PlayableBattleChoice : std::uint8_t { CommandSite, AutoRegion };

enum class PlayableResidence : std::uint8_t { Region, Site, Local, Dungeon };

enum class PlayableEventKind : std::uint8_t {
    NewGame,
    MoveIssued,
    XunAdvanced,
    FactionAiActed,
    EnemyMoved,
    Encounter,
    BattleResolved,
    WorldChanged,
    SiteEntered,
    SiteLeft,
    ConstructionCompleted,
    LocalEntered,
    DoorOpened,
    QuestAccepted,
    QuestCompleted,
    DungeonEntered,
    DungeonCleared,
    LocalLeft,
    TreatySigned,
    ManagedActivity,
};

struct PlayableEvent {
    std::uint64_t id{};
    PlayableEventKind kind{PlayableEventKind::NewGame};
    world::RegionXY tile;
    std::int64_t value_a{};
    std::int64_t value_b{};
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

struct PlayableCoverageSummary {
    PlayableResidence residence{PlayableResidence::Region};
    std::uint16_t development{};
    std::uint16_t order{};
    std::uint64_t production{};
    std::uint32_t city_buildings{};
    std::uint32_t quest_count{};
    bool bandit_quest_available{};
    bool bandit_quest_accepted{};
    bool dungeon_quest_available{};
    bool dungeon_cleared{};
    std::uint16_t dungeon_density_before{};
    std::uint16_t dungeon_density_after{};
    std::uint32_t treaty_count{};
    std::uint16_t last_order_before{};
    std::uint16_t last_order_after{};
    std::uint16_t last_development_before{};
    std::uint16_t last_development_after{};
    std::vector<std::uint64_t> roundtrip_hashes;
    std::uint32_t calibration_n{};
    std::uint64_t manual_total{};
    std::uint64_t managed_total{};
    double signed_relative_error_percent{};
    std::uint32_t persistent_buildings{};
    world::PopulationReduction::Value persistent_population{};
};

} // namespace aetheria::runtime

#include "core/runtime/playable_session_class.h"
