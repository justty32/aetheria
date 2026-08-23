#pragma once

// playable_session.h：把既有 Region 移動、勢力 AI、三層戰鬥與歸約接成最小可玩情境。
// 所有玩法狀態由此純 C++ session 擁有；bridge 與 Godot 只取得批次快照。

#include "core/rules/combat.h"
#include "core/rules/ruleset.h"
#include "core/narrative/emergent_quest.h"
#include "core/site/site_projection.h"
#include "core/time/tick.h"
#include "core/world/combat_scaling.h"
#include "core/world/diplomacy.h"
#include "core/world/named_fate.h"
#include "core/world/region_movement.h"
#include "core/world/region_tiles.h"
#include "core/zone/zone_store.h"
#include "core/zone/zone_manager.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aetheria::runtime {

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

// 三層地圖只把顯示用批次格碼交給 bridge；玩法真值仍留在各 Zone。
struct PlayableGridView {
    std::uint32_t width{};
    std::uint32_t height{};
    std::int8_t z{};
    std::vector<std::uint8_t> cells;
    std::uint16_t player_x{};
    std::uint16_t player_y{};
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

    // M8.2 三層覆蓋情境。親自進入會改變 core 駐留層；代管命令使用同一套
    // core 活動後留在上層。
    void enter_site();
    void leave_site();
    void build_city();
    void manage_city();
    void accept_bandit_quest();
    void enter_local();
    void leave_local();
    void open_door_and_move();
    void suppress_bandits();
    void manage_local();
    void enter_dungeon();
    void leave_dungeon();
    void descend_dungeon();
    void clear_dungeon();
    void manage_dungeon();
    void sign_peace_treaty();
    void measure_site_roundtrips();
    void measure_city_management(std::uint32_t samples = 100);

    [[nodiscard]] PlayableCoverageSummary coverage_summary() const;
    [[nodiscard]] std::optional<PlayableGridView> site_view() const;
    [[nodiscard]] std::optional<PlayableGridView> local_view() const;
    [[nodiscard]] world::RegionXY coverage_tile() const noexcept {
        return coverage_tile_;
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
    void initialize_coverage_site();
    [[nodiscard]] zone::ZoneHandle acquire_coverage_site();
    [[nodiscard]] zone::ZoneHandle acquire_coverage_local();
    [[nodiscard]] std::unique_ptr<zone::Zone>
    materialize_coverage_zone(zone::ZoneKey key,
                              std::unique_ptr<zone::Zone> persistent);
    void perform_city_build(bool managed);
    void perform_bandit_suppression(bool managed);
    void perform_dungeon_clear(bool managed);
    void refresh_quests(zone::Zone& site);
    [[nodiscard]] std::uint64_t
    run_city_economy_sample(std::string_view site_bytes,
                            std::string_view region_bytes) const;

    std::uint64_t seed_{};
    std::uint32_t region_id_{};
    rules::Ruleset ruleset_;
    zone::InMemoryZoneStore store_;
    world::RegionTurnPipeline turn_pipeline_;
    world::WorldDiplomacyState diplomacy_;
    std::unique_ptr<zone::Zone> region_;
    std::optional<zone::Zone> battle_site_;
    std::unique_ptr<zone::ZoneManager> coverage_manager_;
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
    world::RegionXY coverage_tile_{60, 48};
    site::SiteXY local_entrance_{2, 2};
    zone::ZoneKey coverage_site_key_{};
    zone::ZoneKey coverage_local_key_{};
    PlayableResidence residence_{PlayableResidence::Region};
    std::int8_t local_z_{};
    std::uint16_t local_player_x_{31};
    std::uint16_t local_player_y_{32};
    bool local_door_open_{};
    std::vector<narrative::EmergentQuest> quests_;
    std::optional<std::uint64_t> accepted_quest_id_;
    std::uint16_t dungeon_density_before_{};
    std::uint16_t dungeon_density_after_{};
    std::uint16_t last_order_before_{};
    std::uint16_t last_order_after_{};
    std::uint16_t last_development_before_{};
    std::uint16_t last_development_after_{};
    std::vector<std::uint64_t> roundtrip_hashes_;
    std::uint32_t calibration_n_{};
    std::uint64_t calibration_manual_total_{};
    std::uint64_t calibration_managed_total_{};
    double calibration_signed_error_percent_{};
};

} // namespace aetheria::runtime
