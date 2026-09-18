#include "core/site/site_fortifications_detail.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

namespace aetheria::site::fill_detail::fortifications_detail {
namespace {

[[nodiscard]] std::vector<SiteEdgeRef> perimeter(Bounds bounds) {
    std::vector<SiteEdgeRef> result;
    result.reserve((bounds.x1 - bounds.x0 + bounds.y1 - bounds.y0 + 2U) * 2U);
    for (std::uint16_t x = bounds.x0; x <= bounds.x1; ++x) {
        result.push_back({{x, bounds.y0}, SiteBoundarySide::North});
    }
    for (std::uint16_t y = bounds.y0; y <= bounds.y1; ++y) {
        result.push_back({{bounds.x1, y}, SiteBoundarySide::East});
    }
    for (std::uint16_t x = bounds.x1;; --x) {
        result.push_back({{x, bounds.y1}, SiteBoundarySide::South});
        if (x == bounds.x0) {
            break;
        }
    }
    for (std::uint16_t y = bounds.y1;; --y) {
        result.push_back({{bounds.x0, y}, SiteBoundarySide::West});
        if (y == bounds.y0) {
            break;
        }
    }
    return result;
}

[[nodiscard]] bool road_crosses(const SiteSkeleton& skeleton, SiteEdgeRef edge) noexcept {
    const auto offset = offset_for(edge.side);
    const auto other_x = static_cast<std::int32_t>(edge.tile.x) + offset[0];
    const auto other_y = static_cast<std::int32_t>(edge.tile.y) + offset[1];
    if (other_x < 0 || other_y < 0 || other_x >= static_cast<std::int32_t>(kSiteWidth) ||
        other_y >= static_cast<std::int32_t>(kSiteHeight)) {
        return false;
    }
    return skeleton.roads[tile_index(edge.tile.x, edge.tile.y)] != 0 &&
           skeleton.roads[tile_index(static_cast<std::uint16_t>(other_x),
                                     static_cast<std::uint16_t>(other_y))] != 0;
}

[[nodiscard]] std::uint32_t distance_to_gate(SiteEdgeRef edge, const SiteGate& gate) noexcept {
    return static_cast<std::uint32_t>(
        std::abs(static_cast<std::int32_t>(edge.tile.x) - gate.tile.x) +
        std::abs(static_cast<std::int32_t>(edge.tile.y) - gate.tile.y));
}

}  // namespace

void add_ring(SiteProceduralLayer& layer, Bounds bounds, const SiteFastVars& fast,
              const rules::Ruleset& ruleset) {
    const auto& config = ruleset.site_fill_rules().fortification;
    auto segments = perimeter(bounds);
    for (std::size_t index = 0; index < segments.size(); ++index) {
        const bool tower =
            fast.defense >= config.tower_defense && index % config.tower_spacing == 0;
        set_site_edge(layer, segments[index], tower ? config.tower_edge : config.wall_edge);
        layer.wall_edges.push_back(segments[index]);
    }

    std::vector<std::uint8_t> used(segments.size());
    for (const auto& source_gate : layer.skeleton.gates) {
        std::size_t best = segments.size();
        auto best_distance = std::numeric_limits<std::uint32_t>::max();
        for (std::size_t index = 0; index < segments.size(); ++index) {
            if (used[index] != 0 || !road_crosses(layer.skeleton, segments[index])) {
                continue;
            }
            const auto distance = distance_to_gate(segments[index], source_gate);
            if (distance < best_distance) {
                best = index;
                best_distance = distance;
            }
        }
        if (best == segments.size()) {
            throw std::runtime_error{"城牆找不到主幹道穿越點，無法滿足有牆必有門"};
        }
        used[best] = UINT8_C(1);
        set_site_edge(layer, segments[best], config.gate_edge);
        layer.wall_gates.push_back(segments[best]);
    }
    ++layer.wall_ring_count;
}

void add_moat(SiteProceduralLayer& layer, Bounds wall_bounds, const rules::Ruleset& ruleset) {
    if (wall_bounds.x0 == 0 || wall_bounds.y0 == 0 || wall_bounds.x1 + 1U >= kSiteWidth ||
        wall_bounds.y1 + 1U >= kSiteHeight) {
        return;
    }
    const Bounds moat{static_cast<std::uint16_t>(wall_bounds.x0 - 1U),
                      static_cast<std::uint16_t>(wall_bounds.y0 - 1U),
                      static_cast<std::uint16_t>(wall_bounds.x1 + 1U),
                      static_cast<std::uint16_t>(wall_bounds.y1 + 1U)};
    for (const auto edge : perimeter(moat)) {
        if (road_crosses(layer.skeleton, edge)) {
            continue;
        }
        set_site_edge(layer, edge, ruleset.site_fill_rules().fortification.moat_edge);
    }
}

}  // namespace aetheria::site::fill_detail::fortifications_detail
