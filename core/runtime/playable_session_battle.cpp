// playable_session_battle.cpp：遭遇戰鬥結算。

#include "core/runtime/playable_session.h"

#include "core/site/site_reduction.h"


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

[[nodiscard]] constexpr std::uint64_t mix_random_input(std::uint64_t value) noexcept {
    value = (value ^ (value >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31U);
}

[[nodiscard]] std::uint64_t combat_random_seed(std::uint64_t seed,
                                               time::Tick tick,
                                               std::uint64_t seq) noexcept {
    auto value = seed ^ std::rotl(static_cast<std::uint64_t>(
                                      static_cast<std::int64_t>(tick)),
                                  21) ^
                 std::rotl(seq, 43);
    return mix_random_input(value);
}

[[nodiscard]] rules::CombatModifiers neutral_modifiers(
    const rules::CombatRules& rules) noexcept {
    return {rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
            rules.modifier_scale, rules.modifier_scale};
}

[[nodiscard]] std::uint32_t loss_basis_points(std::int32_t loss,
                                              std::int32_t power) noexcept {
    if (power <= 0 || loss <= 0) {
        return 0;
    }
    return static_cast<std::uint32_t>(std::min<std::int64_t>(
        world::kFateBasisPoints,
        static_cast<std::int64_t>(loss) * world::kFateBasisPoints / power));
}

} // namespace

const PlayableBattleReport&
PlayableSession::resolve_encounter(PlayableBattleChoice choice) {
    record_command("resolve_encounter",
                   choice == PlayableBattleChoice::CommandSite
                       ? "choice = \"manual\"\n"
                       : "choice = \"auto\"\n");
    if (!encounter_tile_) {
        throw std::logic_error{"目前沒有可處理的遭遇"};
    }
    auto& player = army(player_army_id_);
    auto& enemy = army(enemy_army_id_);
    const auto& combat_rules = ruleset_.combat_rules();
    const rules::CombatInput input{
        {player.power, neutral_modifiers(combat_rules), {}, 0},
        {enemy.power, neutral_modifiers(combat_rules), {}, 0},
        combat_rules.default_exponent,
        1,
    };
    const auto region_result =
        rules::resolve_region_combat(input, combat_rules);
    const auto layer = choice == PlayableBattleChoice::CommandSite
                           ? world::CombatLayer::Site
                           : world::CombatLayer::Region;
    world::CombatExecutionCounters counters;
    const auto random_seed = combat_random_seed(seed_, now(), current_command_seq_);
    const auto layer_result = world::resolve_scaled_combat(
        input, combat_rules, layer, current_command_seq_, random_seed, {}, {},
        &counters);
    const auto before = tile_state(*encounter_tile_);
    player.power = std::max(0, player.power - layer_result.loss_a);
    enemy.power = std::max(0, enemy.power - layer_result.loss_b);

    auto& site_layers =
        std::get<zone::SitePayload>(battle_site_->payload).layers;
    if (!site_layers.persistent.buildings.empty()) {
        site_layers.persistent.buildings.front().state =
            site::BuildingState::Idle;
    }
    site_layers.persistent.order = site::SiteOrderState{
        .garrison_coverage = 12,
        .patrol_coverage = 6,
        .bandit_pressure = 28,
        .refugee_pressure = 20,
    };
    auto& region_tiles =
        std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    site::reduce_live_site_xun(region_tiles, *encounter_tile_, *battle_site_, ruleset_);

    world::NamedFateLedger ledger;
    ledger.members.push_back({
        .entity_uid = named_commander_uid_,
        .cohort_id = enemy_army_id_.uid,
        .name_key = "守軍隊長艾琳",
        .significance = world::Significance::Site,
        .significance_reason = "battle.commander",
        .modifiers = {},
        .marked = true,
    });
    const auto stage_one = world::FateResolver::apply_stage_one(
        region_tiles, *encounter_tile_,
        {.event_id = current_command_seq_,
         .cohort_id = enemy_army_id_.uid,
         .site_key = zone::value_of(battle_site_->key),
         .base_loss_basis_points = loss_basis_points(layer_result.loss_b,
                                                     input.side_b.power),
         .relief_basis_points = 0,
         .occurred_at = now(),
         .place_key = "暮橋鎮"});
    world::FateExecutionCounters fate_counters;
    const auto fate = world::FateResolver::resolve_present(
        ledger, stage_one, {}, fate_counters);
    const auto named_outcome = fate.decisions.empty()
                                   ? world::FateOutcome::Unharmed
                                   : fate.decisions.front().outcome;

    const bool player_won =
        region_result.outcome == rules::Outcome::SideBRouted ||
        layer_result.loss_b >= layer_result.loss_a;
    if (player_won) {
        region_tiles.owner[region_tiles.index_of(*encounter_tile_)] =
            world::FactionId{1};
    }
    const auto after = tile_state(*encounter_tile_);
    battle_report_ = PlayableBattleReport{
        choice, region_result, layer_result, *encounter_tile_, before, after,
        named_outcome, "守軍隊長艾琳"};
    append_event(PlayableEventKind::BattleResolved, *encounter_tile_,
                 layer_result.loss_a, layer_result.loss_b);
    append_event(PlayableEventKind::WorldChanged, *encounter_tile_,
                 before.population, after.population);
    encounter_tile_.reset();
    ++revision_;
    return *battle_report_;
}

} // namespace aetheria::runtime
