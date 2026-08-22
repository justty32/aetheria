// region_turn.cpp：Region 移動命令發布與旬回合七階段推進（原屬 region_movement.cpp）。

#include "core/world/region_movement.h"
#include "core/world/region_movement_detail.h"
#include "core/world/region_simulation.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace aetheria::world {

TurnClock& turn_clock(zone::Zone& region) {
    auto clocks = region.reg.view<TurnClock>();
    if (clocks.size() != 1U) {
        throw std::runtime_error{"Region 必須恰有一個 TurnClock"};
    }
    return clocks.get<TurnClock>(*clocks.begin());
}

const TurnClock& turn_clock(const zone::Zone& region) {
    const auto clocks = region.reg.view<const TurnClock>();
    if (clocks.size() != 1U) {
        throw std::runtime_error{"Region 必須恰有一個 TurnClock"};
    }
    return clocks.get<const TurnClock>(*clocks.begin());
}

namespace {

[[nodiscard]] const RegionTiles& require_layer(const zone::Zone& region, std::int8_t z) {
    const auto* payload = std::get_if<zone::RegionPayload>(&region.payload);
    if (payload == nullptr || !payload->layers.contains(z)) {
        throw std::invalid_argument{"移動單位指向不存在的 Region layer"};
    }
    return payload->layers.at(z);
}

void notify(const TurnStageObserver& observer, TurnStage stage) {
    if (observer) {
        observer(stage);
    }
}

}  // namespace

void RegionTurnPipeline::issue_move(zone::Zone& region, StableId unit, RegionXY target) const {
    entt::entity found = entt::null;
    for (const auto entity : region.reg.view<const StableId>()) {
        if (region.reg.get<const StableId>(entity) == unit) {
            if (found != entt::null) {
                throw std::runtime_error{"Region 內 StableId 重複：" + std::to_string(unit.uid)};
            }
            found = entity;
        }
    }
    if (found == entt::null || !region.reg.all_of<RegionPosition, MovementPoints>(found)) {
        throw std::invalid_argument{"移動命令的 StableId 不是完整 Region 單位"};
    }
    const auto& position = region.reg.get<const RegionPosition>(found);
    const auto& tiles = require_layer(region, position.z);
    if (!detail::passable(tiles, target, ruleset_)) {
        throw std::invalid_argument{"移動命令目標不可通行"};
    }
    region.reg.emplace_or_replace<RegionMoveCommand>(found, target, false);
}

void RegionTurnPipeline::advance_xun(zone::Zone& region, const TurnStageObserver& observer,
                                     const LiveSiteReductionPass& live_site_reduction,
                                     const FactionAiPass& faction_ai,
                                     const ScriptTurnPass& scripts) const {
    auto& clock = turn_clock(region);
    const auto next = clock.now + time::kXun;
    if (!time::is_representable(next)) {
        throw std::overflow_error{"旬回合推進超出 Tick 可表達範圍"};
    }
    run_xun_stages(region, clock.now, observer, live_site_reduction, faction_ai, scripts);
    clock.now = next;
    region.last_saved_tick = clock.now;
    store_.save(region);
}

void RegionTurnPipeline::settle_elapsed_xun(
    zone::Zone& region, const TurnStageObserver& observer,
    const LiveSiteReductionPass& live_site_reduction,
    const FactionAiPass& faction_ai, const ScriptTurnPass& scripts) const {
    auto& clock = turn_clock(region);
    const auto raw_now = static_cast<std::int64_t>(clock.now);
    const auto raw_xun = static_cast<std::int64_t>(time::kXun);
    if (raw_now % raw_xun != 0) {
        throw std::logic_error{"下層旬結算要求全局時鐘位於旬界"};
    }
    run_xun_stages(region, clock.now - time::kXun, observer, live_site_reduction,
                   faction_ai, scripts);
    region.last_saved_tick = clock.now;
    store_.save(region);
}

void RegionTurnPipeline::run_xun_stages(
    zone::Zone& region, time::Tick simulation_start, const TurnStageObserver& observer,
    const LiveSiteReductionPass& live_site_reduction,
    const FactionAiPass& faction_ai, const ScriptTurnPass& scripts) const {
    if (zone::level_of(region.key) != zone::ZoneLevel::Region) {
        throw std::invalid_argument{"RegionTurnPipeline 只接受 Region zone"};
    }
    auto& payload = std::get<zone::RegionPayload>(region.payload);
    const bool has_live_site = std::ranges::any_of(payload.layers, [](const auto& layer) {
        return std::ranges::any_of(layer.second.site,
                                   [](const SiteState& site) { return site.has_live_site; });
    });
    if (has_live_site && !live_site_reduction) {
        throw std::logic_error{"Region 有 live Site，但本旬未提供歸約 pass"};
    }
    const auto date = time::to_date(simulation_start);

    notify(observer, TurnStage::PlayerCommands);
    for (const auto entity : region.reg.view<RegionMoveCommand>()) {
        region.reg.get<RegionMoveCommand>(entity).collected = true;
    }

    notify(observer, TurnStage::CommandExecution);
    std::vector<std::pair<std::uint64_t, entt::entity>> units;
    for (const auto entity :
         region.reg.view<const StableId, RegionPosition, MovementPoints, RegionMoveCommand>()) {
        units.emplace_back(region.reg.get<const StableId>(entity).uid, entity);
    }
    std::ranges::sort(units);
    for (const auto [uid, entity] : units) {
        static_cast<void>(uid);
        auto& points = region.reg.get<MovementPoints>(entity);
        if (points.per_xun < 0) {
            throw std::runtime_error{"MovementPoints.per_xun 不得為負"};
        }
        points.current = points.per_xun;
        while (region.reg.all_of<RegionMoveCommand>(entity)) {
            auto& position = region.reg.get<RegionPosition>(entity);
            const auto command = region.reg.get<RegionMoveCommand>(entity);
            if (!command.collected) {
                break;
            }
            if (position.tile == command.target) {
                region.reg.remove<RegionMoveCommand>(entity);
                break;
            }
            const auto& tiles = require_layer(region, position.z);
            const auto path =
                find_region_path(tiles, position.tile, command.target, ruleset_, date.season);
            if (!path.has_value() || path->tiles.size() < 2) {
                region.reg.remove<RegionMoveCommand>(entity);
                break;
            }
            const auto cost =
                region_step_cost(tiles, position.tile, path->tiles[1], ruleset_, date.season);
            if (cost > points.current) {
                break;
            }
            points.current -= cost;
            position.tile = path->tiles[1];
        }
    }

    notify(observer, TurnStage::Encounters);
    notify(observer, TurnStage::FactionAi);
    if (faction_ai) {
        faction_ai(simulation_start);
    }
    notify(observer, TurnStage::WorldSimulation);
    if (has_live_site) {
        live_site_reduction(region);
    }
    for (auto& [z, tiles] : payload.layers) {
        static_cast<void>(z);
        static_cast<void>(RegionSimulation::advance_xun(tiles));
    }
    notify(observer, TurnStage::Events);
    if (scripts) {
        scripts(simulation_start);
    }
    notify(observer, TurnStage::TurnEnd);
}

}  // namespace aetheria::world
