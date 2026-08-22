// playable_session.cpp：最小可玩情境的權威 core 編排。

#include "core/runtime/playable_session.h"

#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "core/world/faction_ai.h"
#include "core/worldgen/region_generator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace aetheria::runtime {
namespace {

[[nodiscard]] rules::CombatModifiers neutral_modifiers(
    const rules::CombatRules& rules) noexcept {
    return {rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
            rules.modifier_scale, rules.modifier_scale};
}

[[nodiscard]] std::uint8_t action_value(ai::FactionActionKind action) noexcept {
    return static_cast<std::uint8_t>(action);
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

PlayableSession::PlayableSession(std::uint64_t seed, std::uint32_t region_id,
                                 std::string data_directory)
    : seed_{seed}, region_id_{region_id},
      ruleset_{rules::RulesetLoader::load(data_directory)}, store_{ruleset_},
      turn_pipeline_{ruleset_, store_}, diplomacy_{3, seed, ruleset_} {
    initialize_scenario();
    initialize_diplomacy();
}

void PlayableSession::initialize_scenario() {
    const auto build = worldgen::build_skeleton(
        worldgen::RegionSlowVariables{region_id_, 128, 96}, seed_, ruleset_);
    auto generated = worldgen::populate(build.skeleton,
                                        worldgen::RegionFastVariables{});
    const auto grass = ruleset_.find_terrain("terrain.grassland");
    const auto plain = ruleset_.find_relief("relief.plain");
    const auto no_feature = ruleset_.find_feature("feature.none");
    const auto no_edge = ruleset_.find_edge("edge.none");
    if (!grass || !plain || !no_feature || !no_edge) {
        throw std::runtime_error{"可玩情境缺少草原／平地／空地物／空邊規則"};
    }
    for (std::int16_t x = player_start_.x; x <= enemy_start_.x; ++x) {
        const world::RegionXY coordinate{x, player_start_.y};
        const auto index = generated.index_of(coordinate);
        generated.base[index] = *grass;
        generated.relief[index] = *plain;
        generated.feature[index] = *no_feature;
        generated.owner[index] = x < battle_tile_.x ? world::FactionId{1}
                                                     : world::FactionId{2};
        for (std::size_t direction = 0; direction < 4; ++direction) {
            generated.edges[index * 4U + direction] = *no_edge;
        }
    }
    for (std::int16_t x = player_start_.x; x < enemy_start_.x; ++x) {
        generated.set_edge({x, player_start_.y},
                           {static_cast<std::int16_t>(x + 1), player_start_.y},
                           *no_edge);
    }
    generated.settlement[generated.index_of(battle_tile_)] =
        world::SettlementTier::Town;

    const auto key = zone::child_key(zone::kRootZone, region_id_, 0);
    region_ = std::make_unique<zone::Zone>(key);
    std::get<zone::RegionPayload>(region_->payload).layers.emplace(
        0, std::move(generated));
    const auto placeholder = *region_->reg.view<zone::ZoneMeta>().begin();
    region_->reg.emplace<world::TurnClock>(placeholder, time::Tick{0});

    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    battle_site_.emplace(site::materialize_site_zone(
        region_tiles, battle_tile_, seed_, region_id_, ruleset_));
    site::reduce_live_site_xun(region_tiles, battle_tile_, *battle_site_);

    const auto create_army = [&](PlayableArmy army_value, world::RegionXY position,
                                 world::RegionXY target) {
        const auto entity = region_->reg.create();
        region_->reg.emplace<world::StableId>(entity, army_value.id);
        region_->reg.emplace<world::RegionPosition>(entity, 0, position);
        region_->reg.emplace<world::MovementPoints>(entity, 0, 4);
        armies_.push_back(army_value);
        if (!army_value.player_controlled) {
            turn_pipeline_.issue_move(*region_, army_value.id, target);
        }
    };
    create_army({player_army_id_, world::FactionId{1}, 120'000, true},
                player_start_, enemy_start_);
    create_army({enemy_army_id_, world::FactionId{2}, 48'000, false},
                enemy_start_, player_start_);
    append_event(PlayableEventKind::NewGame, player_start_, region_id_, seed_);
}

void PlayableSession::initialize_diplomacy() {
    diplomacy_.set_faction_truth(world::FactionId{1}, 120'000, 80'000);
    diplomacy_.set_faction_truth(world::FactionId{2}, 48'000, 42'000);
    diplomacy_.set_faction_truth(world::FactionId{3}, 60'000, 70'000);
    for (std::uint16_t observer = 1; observer <= 3; ++observer) {
        for (std::uint16_t target = 1; target <= 3; ++target) {
            if (observer != target) {
                diplomacy_.observe_faction(world::FactionId{observer},
                                           world::FactionId{target}, 0,
                                           time::Tick{0}, 4);
            }
        }
    }
    world::set_managed_faction_goal(diplomacy_, world::FactionId{2},
                                    ai::FactionGoal::Conquer);
}

world::RegionPosition& PlayableSession::position_of(world::StableId unit) {
    for (const auto entity :
         region_->reg.view<const world::StableId, world::RegionPosition>()) {
        if (region_->reg.get<const world::StableId>(entity) == unit) {
            return region_->reg.get<world::RegionPosition>(entity);
        }
    }
    throw std::invalid_argument{"找不到指定的 Region 部隊"};
}

const world::RegionPosition&
PlayableSession::position_of(world::StableId unit) const {
    for (const auto entity :
         region_->reg.view<const world::StableId, const world::RegionPosition>()) {
        if (region_->reg.get<const world::StableId>(entity) == unit) {
            return region_->reg.get<const world::RegionPosition>(entity);
        }
    }
    throw std::invalid_argument{"找不到指定的 Region 部隊"};
}

PlayableArmy& PlayableSession::army(world::StableId unit) {
    const auto found =
        std::ranges::find(armies_, unit, &PlayableArmy::id);
    if (found == armies_.end()) {
        throw std::invalid_argument{"找不到指定的部隊資料"};
    }
    return *found;
}

const PlayableArmy& PlayableSession::army(world::StableId unit) const {
    const auto found =
        std::ranges::find(armies_, unit, &PlayableArmy::id);
    if (found == armies_.end()) {
        throw std::invalid_argument{"找不到指定的部隊資料"};
    }
    return *found;
}

void PlayableSession::append_event(PlayableEventKind kind, world::RegionXY tile,
                                   std::int64_t value_a,
                                   std::int64_t value_b) {
    events_.push_back({next_event_id_++, kind, tile, value_a, value_b});
}

void PlayableSession::issue_move(world::StableId unit,
                                 world::RegionXY target) {
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
            site::reduce_live_site_xun(reducing_tiles, battle_tile_,
                                       *battle_site_);
        },
        [&](time::Tick tick) {
            for (const auto faction : {world::FactionId{2}, world::FactionId{3}}) {
                const auto ai_report = world::advance_faction_ai_xun(
                    diplomacy_, faction, {4, 0, true, true, faction == world::FactionId{2}},
                    tick, ruleset_);
                const auto action = action_value(ai_report.decision.command.kind);
                report.ai_actions.push_back(action);
                append_event(PlayableEventKind::FactionAiActed, {},
                             static_cast<std::int64_t>(static_cast<std::uint16_t>(faction)),
                             action);
            }
        });
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

const PlayableBattleReport&
PlayableSession::resolve_encounter(PlayableBattleChoice choice) {
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
    const auto layer_result = world::resolve_scaled_combat(
        input, combat_rules, layer, next_event_id_, seed_ + revision_, {}, {},
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
    site::reduce_live_site_xun(region_tiles, *encounter_tile_, *battle_site_);

    world::NamedFateLedger ledger;
    ledger.members.push_back({
        .entity_uid = 9001,
        .cohort_id = enemy_army_id_.uid,
        .name_key = "守軍隊長艾琳",
        .significance = world::Significance::Site,
        .significance_reason = "battle.commander",
        .modifiers = {},
        .marked = true,
    });
    const auto stage_one = world::FateResolver::apply_stage_one(
        region_tiles, *encounter_tile_,
        {.event_id = next_event_id_,
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

const world::RegionTiles& PlayableSession::tiles() const {
    return std::get<zone::RegionPayload>(region_->payload).layers.at(0);
}

std::vector<PlayableArmyView> PlayableSession::armies() const {
    std::vector<PlayableArmyView> result;
    result.reserve(armies_.size());
    for (const auto& value : armies_) {
        result.push_back({value.id, value.faction, position_of(value.id).tile,
                          value.power, value.player_controlled});
    }
    return result;
}

PlayableTileState PlayableSession::tile_state(world::RegionXY tile) const {
    const auto& region_tiles = tiles();
    const auto index = region_tiles.index_of(tile);
    return {region_tiles.owner[index],
            region_tiles.reduction_value<world::PopulationReduction>(tile),
            region_tiles.reduction_value<world::OrderReduction>(tile)};
}

time::Tick PlayableSession::now() const {
    return world::turn_clock(*region_).now;
}

} // namespace aetheria::runtime
