#include "core/worldgen/influence_spread.h"

#include "core/world/region_movement.h"

#include <limits>
#include <stdexcept>

namespace aetheria::worldgen {

std::int32_t influence_terrain_step_cost(const rules::Ruleset& ruleset,
                                         InfluenceTerrainStepInput input) {
    const auto* terrain = ruleset.terrain(input.terrain);
    const auto* relief = ruleset.relief(input.relief);
    const auto* feature = ruleset.feature(input.feature);
    const auto& movement = ruleset.movement_rules();
    if (terrain == nullptr || relief == nullptr || feature == nullptr) {
        throw std::runtime_error{"影響力成本含不存在的 terrain／relief／feature"};
    }
    if (!movement.loaded || input.season < 1 ||
        input.season > movement.season_numerators.size() || movement.season_denominator == 0) {
        throw std::invalid_argument{"影響力成本需要有效 movement.toml 與季節"};
    }
    const auto base = static_cast<std::int64_t>(terrain->move_cost) + relief->move_cost +
                      feature->move_cost;
    const auto scaled = base * world::kMovementPointScale;
    const auto numerator = movement.season_numerators[input.season - 1U];
    const auto adjusted =
        (scaled * numerator + movement.season_denominator - 1U) /
        movement.season_denominator;
    if (base <= 0 || adjusted <= 0 || adjusted > std::numeric_limits<std::int32_t>::max()) {
        throw std::overflow_error{"影響力 terrain-only 成本超出正 int32"};
    }
    return static_cast<std::int32_t>(adjusted);
}

std::vector<world::FactionId>
spread_influence(const world::RegionTiles& tiles, std::span<const InfluenceCapital> capitals,
                 const rules::Ruleset& ruleset,
                 const rules::CivilizationRules::FactionRules& factions,
                 InfluenceSpreadDiagnostics* diagnostics) {
    if (factions.governance_max_cost < 0) {
        throw std::invalid_argument{"治理距離不得為負"};
    }
    const auto claims =
        claim_all_land(tiles, capitals, ruleset, factions.influence_season, diagnostics);
    return release_beyond_governance(claims, factions.governance_max_cost);
}

}  // namespace aetheria::worldgen
