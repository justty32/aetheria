// region_step_cost.cpp：單步移動成本與季節下限成本（原屬 region_movement.cpp）。

#include "core/world/region_movement.h"
#include "core/world/region_movement_detail.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace aetheria::world {
namespace {

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

}  // namespace

std::int32_t region_step_cost(const RegionTiles& tiles, RegionXY from, RegionXY to,
                              const rules::Ruleset& ruleset, std::uint8_t season) {
    if (!detail::in_bounds(tiles, from) || !detail::passable(tiles, to, ruleset) ||
        detail::manhattan(from, to) != 1U) {
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

}  // namespace aetheria::world
