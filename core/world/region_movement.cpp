#include "core/world/region_movement.h"

#include "core/base/check.h"

#include <algorithm>
#include <array>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <tuple>

namespace aetheria::world {
namespace {

[[nodiscard]] std::array<RegionXY, 4> neighbors(RegionXY tile) noexcept {
    return {{{tile.x, static_cast<std::int16_t>(tile.y - 1)},
             {static_cast<std::int16_t>(tile.x + 1), tile.y},
             {tile.x, static_cast<std::int16_t>(tile.y + 1)},
             {static_cast<std::int16_t>(tile.x - 1), tile.y}}};
}

[[nodiscard]] bool in_bounds(const RegionTiles& tiles, RegionXY tile) noexcept {
    return tile.x >= 0 && tile.y >= 0 && static_cast<std::uint32_t>(tile.x) < tiles.width &&
           static_cast<std::uint32_t>(tile.y) < tiles.height;
}

[[nodiscard]] bool passable(const RegionTiles& tiles, RegionXY tile,
                            const rules::Ruleset& ruleset) {
    if (!in_bounds(tiles, tile)) {
        return false;
    }
    const auto* terrain = ruleset.terrain(tiles.base.at(tiles.index_of(tile)));
    if (terrain == nullptr) {
        throw std::runtime_error{"RegionTiles 含不存在的 TerrainId"};
    }
    return (terrain->flags & rules::kTerrainWaterFlag) == 0;
}

[[nodiscard]] std::int32_t apply_season(std::int64_t scaled_cost,
                                        const rules::MovementRules& movement, std::uint8_t season) {
    if (!movement.loaded || season < 1 || season > movement.season_numerators.size() ||
        movement.season_denominator == 0) {
        throw std::invalid_argument{"Ruleset 缺少有效 movement.toml 或季節無效"};
    }
    const auto numerator = movement.season_numerators[season - 1U];
    const auto adjusted =
        (scaled_cost * numerator + movement.season_denominator - 1U) / movement.season_denominator;
    if (adjusted <= 0 || adjusted > std::numeric_limits<std::int32_t>::max()) {
        throw std::overflow_error{"Region 移動成本超出正 int32"};
    }
    return static_cast<std::int32_t>(adjusted);
}

[[nodiscard]] std::int32_t tile_base_cost(const RegionTiles& tiles, RegionXY to,
                                          const rules::Ruleset& ruleset) {
    const auto index = tiles.index_of(to);
    const auto* terrain = ruleset.terrain(tiles.base.at(index));
    const auto* relief = ruleset.relief(tiles.relief.at(index));
    const auto* feature = ruleset.feature(tiles.feature.at(index));
    if (terrain == nullptr || relief == nullptr || feature == nullptr) {
        throw std::runtime_error{"RegionTiles 移動 def 下標無效"};
    }
    const auto cost =
        static_cast<std::int64_t>(terrain->move_cost) + relief->move_cost + feature->move_cost;
    if (cost <= 0 || cost > std::numeric_limits<std::int32_t>::max() / kMovementPointScale) {
        throw std::overflow_error{"tile 移動成本超出範圍"};
    }
    return static_cast<std::int32_t>(cost * kMovementPointScale);
}

[[nodiscard]] std::uint32_t manhattan(RegionXY lhs, RegionXY rhs) noexcept {
    return static_cast<std::uint32_t>(std::abs(static_cast<int>(lhs.x) - static_cast<int>(rhs.x)) +
                                      std::abs(static_cast<int>(lhs.y) - static_cast<int>(rhs.y)));
}

[[nodiscard]] TurnClock& require_clock(zone::Zone& region) {
    auto clocks = region.reg.view<TurnClock>();
    if (clocks.size() != 1U) {
        throw std::runtime_error{"Region 必須恰有一個 TurnClock"};
    }
    return clocks.get<TurnClock>(*clocks.begin());
}

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

std::int32_t region_step_cost(const RegionTiles& tiles, RegionXY from, RegionXY to,
                              const rules::Ruleset& ruleset, std::uint8_t season) {
    if (!in_bounds(tiles, from) || !passable(tiles, to, ruleset) || manhattan(from, to) != 1U) {
        throw std::invalid_argument{"Region step 必須是界內、可通行的四鄰接移動"};
    }
    const auto edge_id = tiles.edge_between(from, to);
    const auto* edge = ruleset.edge(edge_id);
    if (edge == nullptr) {
        throw std::runtime_error{"RegionTiles 含不存在的 EdgeId"};
    }
    const auto no_edge = ruleset.find_edge("edge.none");
    if (!no_edge.has_value()) {
        throw std::runtime_error{"Ruleset 缺少 edge.none"};
    }
    const auto tile_cost = tile_base_cost(tiles, to, ruleset);
    const bool is_river = (edge->flags & rules::kEdgeRiverFlag) != 0;
    const bool has_bridge = (edge->flags & rules::kEdgeBridgeFlag) != 0;
    const auto scaled = edge_id == *no_edge ? static_cast<std::int64_t>(tile_cost)
                        : is_river && !has_bridge
                            ? static_cast<std::int64_t>(tile_cost) +
                                  static_cast<std::int64_t>(edge->move_cost) * kMovementPointScale
                            : static_cast<std::int64_t>(edge->move_cost) * kMovementPointScale;
    return apply_season(scaled, ruleset.movement_rules(), season);
}

std::int32_t minimum_region_step_cost(const rules::Ruleset& ruleset, std::uint8_t season) {
    if (ruleset.terrains().empty() || ruleset.reliefs().empty() || ruleset.features().empty() ||
        ruleset.edges().empty()) {
        throw std::invalid_argument{"Ruleset 缺少移動定義"};
    }
    const auto min_move_cost = [](const auto defs) {
        return std::ranges::min(defs, {}, &std::remove_cvref_t<decltype(defs.front())>::move_cost)
            .move_cost;
    };
    auto minimum = static_cast<std::int64_t>(min_move_cost(ruleset.terrains())) +
                   min_move_cost(ruleset.reliefs()) + min_move_cost(ruleset.features());
    const auto no_edge = ruleset.find_edge("edge.none");
    for (std::size_t index = 0; index < ruleset.edges().size(); ++index) {
        const auto id = static_cast<rules::EdgeId>(index);
        if (no_edge.has_value() && id == *no_edge) {
            continue;
        }
        const auto& edge = ruleset.edges()[index];
        if ((edge.flags & rules::kEdgeRiverFlag) != 0 &&
            (edge.flags & rules::kEdgeBridgeFlag) == 0) {
            continue;
        }
        minimum = std::min<std::int64_t>(minimum, edge.move_cost);
    }
    return apply_season(minimum * kMovementPointScale, ruleset.movement_rules(), season);
}

std::optional<RegionPath> find_region_path(const RegionTiles& tiles, RegionXY start, RegionXY goal,
                                           const rules::Ruleset& ruleset, std::uint8_t season,
                                           std::uint32_t heuristic_multiplier) {
    if (!passable(tiles, start, ruleset) || !passable(tiles, goal, ruleset)) {
        return std::nullopt;
    }
    const auto count = tiles.tile_count();
    const auto start_index = tiles.index_of(start);
    const auto goal_index = tiles.index_of(goal);
    constexpr auto infinity = std::numeric_limits<std::int64_t>::max();
    constexpr auto missing = std::numeric_limits<std::size_t>::max();
    std::vector<std::int64_t> distance(count, infinity);
    std::vector<std::size_t> parent(count, missing);
    using Candidate = std::tuple<std::int64_t, std::int64_t, std::size_t>;
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> open;
    const auto minimum = minimum_region_step_cost(ruleset, season);
    distance[start_index] = 0;
    open.emplace(static_cast<std::int64_t>(manhattan(start, goal)) * minimum * heuristic_multiplier,
                 0, start_index);

    while (!open.empty()) {
        const auto [estimate, known_cost, current] = open.top();
        static_cast<void>(estimate);
        open.pop();
        if (known_cost != distance[current]) {
            continue;
        }
        if (current == goal_index) {
            break;
        }
        const RegionXY from{static_cast<std::int16_t>(current % tiles.width),
                            static_cast<std::int16_t>(current / tiles.width)};
        for (const auto to : neighbors(from)) {
            if (!passable(tiles, to, ruleset)) {
                continue;
            }
            const auto next = tiles.index_of(to);
            const auto candidate = known_cost + region_step_cost(tiles, from, to, ruleset, season);
            if (candidate >= distance[next]) {
                continue;
            }
            distance[next] = candidate;
            parent[next] = current;
            const auto heuristic =
                static_cast<std::int64_t>(manhattan(to, goal)) * minimum * heuristic_multiplier;
            open.emplace(candidate + heuristic, candidate, next);
        }
    }
    if (distance[goal_index] == infinity) {
        return std::nullopt;
    }
    RegionPath result{{}, distance[goal_index]};
    for (auto current = goal_index;; current = parent[current]) {
        result.tiles.push_back({static_cast<std::int16_t>(current % tiles.width),
                                static_cast<std::int16_t>(current / tiles.width)});
        if (current == start_index) {
            break;
        }
        if (parent[current] == missing) {
            throw std::runtime_error{"A* parent chain 中斷"};
        }
    }
    std::ranges::reverse(result.tiles);
    return result;
}

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
    if (!passable(tiles, target, ruleset_)) {
        throw std::invalid_argument{"移動命令目標不可通行"};
    }
    region.reg.emplace_or_replace<RegionMoveCommand>(found, target, false);
}

void RegionTurnPipeline::advance_xun(zone::Zone& region, const TurnStageObserver& observer) const {
    if (zone::level_of(region.key) != zone::ZoneLevel::Region) {
        throw std::invalid_argument{"RegionTurnPipeline 只接受 Region zone"};
    }
    auto& clock = require_clock(region);
    const auto date = time::to_date(clock.now);

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
    notify(observer, TurnStage::WorldSimulation);
    notify(observer, TurnStage::Events);
    notify(observer, TurnStage::TurnEnd);
    const auto next = clock.now + time::kXun;
    if (!time::is_representable(next)) {
        throw std::overflow_error{"旬回合推進超出 Tick 可表達範圍"};
    }
    clock.now = next;
    region.last_saved_tick = clock.now;
    store_.save(region);
}

}  // namespace aetheria::world
