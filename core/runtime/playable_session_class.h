#pragma once

// 類別宣告；既有 playable_session.h 是彙整入口。
#include "core/runtime/playable_session.h"

namespace aetheria::runtime {

// PlayableSession 是 M8.1 的 core 編排門面。呼叫端獨占 session；所有 getter
// 回傳的參考在下一個 mutating command 或 session 析構前有效。
class PlayableSession {
public:
    PlayableSession(std::uint64_t seed, std::uint32_t region_id,
                    std::string data_directory, zone::ZoneStore& store);

    [[nodiscard]] static std::unique_ptr<PlayableSession>
    load(std::filesystem::path slot_directory, zone::ZoneStore& store,
         std::string_view character_name);

    PlayableSession(const PlayableSession&) = delete;
    PlayableSession& operator=(const PlayableSession&) = delete;
    ~PlayableSession();

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
    void save_game(zone::FileZoneStore& destination,
                   std::string_view character_name);
    [[nodiscard]] CharacterState export_character_state() const;
    void import_character_state(const CharacterState& state);
    // sim replay 使用空的 session，從指定世界槽的 journal 重放全部玩家輸入。
    void replay_history_from(const std::filesystem::path& slot_directory);
    [[nodiscard]] std::uint64_t history_head_hash() const noexcept;
    [[nodiscard]] std::uint64_t history_head_seq() const noexcept;
    void set_interrupt_after_journal_for_testing(bool enabled);
    void set_interrupt_after_save_for_testing(bool enabled);
    [[nodiscard]] const world::WorldDiplomacyState& diplomacy() const noexcept {
        return *diplomacy_;
    }

private:
    friend class TurnCommit;
    struct LoadTag {};
    PlayableSession(LoadTag, std::filesystem::path slot_directory,
                    zone::ZoneStore& store);
    [[nodiscard]] world::RegionPosition& position_of(world::StableId unit);
    [[nodiscard]] const world::RegionPosition& position_of(world::StableId unit) const;
    [[nodiscard]] world::ArmyState& army(world::StableId unit);
    [[nodiscard]] const world::ArmyState& army(world::StableId unit) const;
    void append_event(PlayableEventKind kind, world::RegionXY tile = {},
                      std::int64_t value_a = 0, std::int64_t value_b = 0);
    void record_command(std::string_view kind, std::string payload = {});
    void perform_enter_site();
    void perform_leave_site();
    void detect_encounter();
    void initialize_scenario();
    void initialize_loaded_scenario();
    void initialize_diplomacy();
    void initialize_coverage_site();
    [[nodiscard]] zone::ZoneHandle acquire_coverage_site();
    [[nodiscard]] zone::ZoneHandle acquire_coverage_local();
    [[nodiscard]] std::unique_ptr<zone::Zone>
    materialize_session_zone(zone::ZoneKey key,
                             std::unique_ptr<zone::Zone> persistent);
    void bind_loaded_zone(zone::ZoneHandle handle, zone::Zone*& destination,
                          std::string_view description);
    void rebuild_army_handles();
    void perform_city_build(bool managed);
    void perform_bandit_suppression(bool managed);
    void perform_dungeon_clear(bool managed);
    void refresh_quests(zone::Zone& site);
    [[nodiscard]] std::uint64_t
    run_city_economy_sample(std::string_view site_bytes,
                            std::string_view region_bytes) const;

    std::uint64_t seed_{};
    std::uint32_t region_id_{};
    std::filesystem::path base_raws_directory_;
    std::optional<std::filesystem::path> character_save_path_;
    rules::Ruleset ruleset_;
    zone::ZoneStore& store_;
    world::RegionTurnPipeline turn_pipeline_;
    std::unique_ptr<zone::ZoneManager> manager_;
    std::unique_ptr<TurnCommit> turn_commit_;
    world::WorldDiplomacyState* diplomacy_{};
    zone::Zone* region_{};
    zone::Zone* battle_site_{};
    zone::ZoneKey region_key_{};
    zone::ZoneKey battle_site_key_{};
    std::vector<PlayableArmy> armies_;
    std::optional<world::RegionXY> encounter_tile_;
    std::optional<PlayableBattleReport> battle_report_;
    std::vector<PlayableEvent> events_;
    std::uint64_t next_event_id_{1};
    std::uint64_t revision_{1};
    world::StableId player_army_id_{};
    world::StableId enemy_army_id_{};
    std::uint64_t named_commander_uid_{};
    std::uint64_t current_command_seq_{};
    bool replaying_history_{};
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
