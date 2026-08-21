#include "core/site/site_fill_detail.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

namespace aetheria::site::fill_detail {
namespace {

[[nodiscard]] std::uint32_t distance(SiteXY tile, SiteEdgeRef edge) noexcept {
    return static_cast<std::uint32_t>(std::abs(static_cast<std::int32_t>(tile.x) - edge.tile.x) +
                                      std::abs(static_cast<std::int32_t>(tile.y) - edge.tile.y));
}

[[nodiscard]] std::uint32_t nearest_distance(SiteXY tile,
                                             const std::vector<SiteEdgeRef>& sources) noexcept {
    auto result = std::numeric_limits<std::uint32_t>::max();
    for (const auto source : sources) {
        result = std::min(result, distance(tile, source));
    }
    return sources.empty() ? 0U : result;
}

[[nodiscard]] SiteXY building_center(const ProceduralBuilding& building) noexcept {
    return {static_cast<std::uint16_t>(building.origin.x + building.width / 2U),
            static_cast<std::uint16_t>(building.origin.y + building.height / 2U)};
}

[[nodiscard]] bool is_gate(const SiteProceduralLayer& layer, SiteEdgeRef edge) noexcept {
    return std::ranges::find(layer.wall_gates, edge) != layer.wall_gates.end();
}

}  // namespace

void apply_site_damage(SiteProceduralLayer& layer, const SiteFastVars& fast,
                       const rules::Ruleset& ruleset) {
    if (fast.damage == 0 || layer.buildings.empty()) {
        return;
    }
    const auto no_edge = ruleset.find_edge("edge.none");
    if (!no_edge.has_value()) {
        throw std::runtime_error{"Site 損毀缺少 edge.none"};
    }

    std::vector<SiteEdgeRef> breach_candidates;
    for (const auto edge : layer.wall_edges) {
        if (!is_gate(layer, edge)) {
            breach_candidates.push_back(edge);
        }
    }
    const auto breach_scale = ruleset.site_fill_rules().fortification.breach_percent_at_full_damage;
    const auto breach_numerator =
        static_cast<std::uint64_t>(breach_candidates.size()) * fast.damage * breach_scale;
    const auto breach_count =
        breach_candidates.empty()
            ? 0U
            : std::max<std::size_t>(1U,
                                    static_cast<std::size_t>((breach_numerator + 9999U) / 10000U));
    const auto seed = hash_site_skeleton(layer.skeleton);
    std::ranges::sort(breach_candidates, [&](SiteEdgeRef left, SiteEdgeRef right) {
        const auto left_distance = nearest_distance(left.tile, layer.wall_gates);
        const auto right_distance = nearest_distance(right.tile, layer.wall_gates);
        if (left_distance != right_distance) {
            return left_distance < right_distance;
        }
        const auto left_tie = worldgen::splitmix64(seed ^ tile_index(left.tile.x, left.tile.y) ^
                                                   static_cast<std::uint8_t>(left.side));
        const auto right_tie = worldgen::splitmix64(seed ^ tile_index(right.tile.x, right.tile.y) ^
                                                    static_cast<std::uint8_t>(right.side));
        return left_tie < right_tie;
    });
    for (std::size_t index = 0; index < std::min(breach_count, breach_candidates.size()); ++index) {
        set_site_edge(layer, breach_candidates[index], *no_edge);
        layer.wall_breaches.push_back(breach_candidates[index]);
    }

    std::vector<SiteEdgeRef> attack_sources = layer.wall_gates;
    attack_sources.insert(attack_sources.end(), layer.wall_breaches.begin(),
                          layer.wall_breaches.end());
    std::vector<std::size_t> ranked(layer.buildings.size());
    for (std::size_t index = 0; index < ranked.size(); ++index) {
        ranked[index] = index;
    }
    std::ranges::sort(ranked, [&](std::size_t left, std::size_t right) {
        const auto left_distance =
            nearest_distance(building_center(layer.buildings[left]), attack_sources);
        const auto right_distance =
            nearest_distance(building_center(layer.buildings[right]), attack_sources);
        if (left_distance != right_distance) {
            return left_distance < right_distance;
        }
        return worldgen::splitmix64(seed ^ left) < worldgen::splitmix64(seed ^ right);
    });
    const auto damage_count = std::min<std::size_t>(
        ranked.size(), (ranked.size() * static_cast<std::size_t>(fast.damage) + 99U) / 100U);
    for (std::size_t rank = 0; rank < damage_count; ++rank) {
        auto& building = layer.buildings[ranked[rank]];
        const auto sample = worldgen::splitmix64(seed ^ ranked[rank] ^ UINT64_C(0xD4A463));
        building.damage = (sample & 1U) == 0 ? ProceduralBuildingDamage::Rubble
                                             : ProceduralBuildingDamage::Burned;
    }
}

}  // namespace aetheria::site::fill_detail
