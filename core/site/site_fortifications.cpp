#include "core/site/site_fill_detail.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

namespace aetheria::site::fill_detail {
namespace {

struct Bounds {
    std::uint16_t x0{};
    std::uint16_t y0{};
    std::uint16_t x1{};
    std::uint16_t y1{};

    constexpr bool operator==(const Bounds&) const noexcept = default;
};

[[nodiscard]] constexpr SiteBoundarySide opposite(SiteBoundarySide side) noexcept {
    switch (side) {
    case SiteBoundarySide::North:
        return SiteBoundarySide::South;
    case SiteBoundarySide::East:
        return SiteBoundarySide::West;
    case SiteBoundarySide::South:
        return SiteBoundarySide::North;
    case SiteBoundarySide::West:
        return SiteBoundarySide::East;
    }
    return SiteBoundarySide::North;
}

[[nodiscard]] constexpr std::array<std::int16_t, 2> offset_for(SiteBoundarySide side) noexcept {
    switch (side) {
    case SiteBoundarySide::North:
        return {0, -1};
    case SiteBoundarySide::East:
        return {1, 0};
    case SiteBoundarySide::South:
        return {0, 1};
    case SiteBoundarySide::West:
        return {-1, 0};
    }
    return {};
}

[[nodiscard]] std::uint32_t center_distance(const SiteSkeleton& skeleton,
                                            const SiteBlock& block) noexcept {
    const auto x = static_cast<std::int32_t>(block.origin.x) + block.width / 2;
    const auto y = static_cast<std::int32_t>(block.origin.y) + block.height / 2;
    return static_cast<std::uint32_t>(
        std::abs(x - static_cast<std::int32_t>(skeleton.city_center.x)) +
        std::abs(y - static_cast<std::int32_t>(skeleton.city_center.y)));
}

[[nodiscard]] std::vector<std::size_t> important_blocks(const SiteProceduralLayer& layer) {
    std::vector<std::size_t> result;
    for (std::size_t index = 0; index < layer.block_zoning.size(); ++index) {
        if (layer.block_zoning[index] != SiteZoning::Open) {
            result.push_back(index);
        }
    }
    std::ranges::sort(result, [&](std::size_t left, std::size_t right) {
        const auto left_zone = layer.block_zoning[left];
        const auto right_zone = layer.block_zoning[right];
        const auto left_priority = left_zone == SiteZoning::Commercial ? 0U : 1U;
        const auto right_priority = right_zone == SiteZoning::Commercial ? 0U : 1U;
        const auto left_distance = center_distance(layer.skeleton, layer.skeleton.blocks[left]);
        const auto right_distance = center_distance(layer.skeleton, layer.skeleton.blocks[right]);
        if (left_priority != right_priority) {
            return left_priority < right_priority;
        }
        return left_distance != right_distance ? left_distance < right_distance : left < right;
    });
    return result;
}

[[nodiscard]] Bounds bounds_for(const SiteProceduralLayer& layer,
                                const std::vector<std::size_t>& ranked, std::size_t count) {
    auto x0 = static_cast<std::uint16_t>(kSiteWidth - 1U);
    auto y0 = static_cast<std::uint16_t>(kSiteHeight - 1U);
    std::uint16_t x1{};
    std::uint16_t y1{};
    for (std::size_t rank = 0; rank < count; ++rank) {
        const auto& block = layer.skeleton.blocks[ranked[rank]];
        x0 = std::min(x0, block.origin.x);
        y0 = std::min(y0, block.origin.y);
        x1 = std::max<std::uint16_t>(x1,
                                     static_cast<std::uint16_t>(block.origin.x + block.width - 1U));
        y1 = std::max<std::uint16_t>(
            y1, static_cast<std::uint16_t>(block.origin.y + block.height - 1U));
    }
    return {static_cast<std::uint16_t>(x0 > 1U ? x0 - 1U : 1U),
            static_cast<std::uint16_t>(y0 > 1U ? y0 - 1U : 1U),
            static_cast<std::uint16_t>(std::min<std::uint32_t>(x1 + 1U, kSiteWidth - 2U)),
            static_cast<std::uint16_t>(std::min<std::uint32_t>(y1 + 1U, kSiteHeight - 2U))};
}

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

}  // namespace

void set_site_edge(SiteProceduralLayer& layer, SiteEdgeRef edge, rules::EdgeId kind) {
    const auto side = static_cast<std::size_t>(edge.side);
    layer.edges.at(tile_index(edge.tile.x, edge.tile.y) * 4U + side) = kind;
    const auto offset = offset_for(edge.side);
    const auto other_x = static_cast<std::int32_t>(edge.tile.x) + offset[0];
    const auto other_y = static_cast<std::int32_t>(edge.tile.y) + offset[1];
    if (other_x < 0 || other_y < 0 || other_x >= static_cast<std::int32_t>(kSiteWidth) ||
        other_y >= static_cast<std::int32_t>(kSiteHeight)) {
        return;
    }
    layer.edges.at(
        tile_index(static_cast<std::uint16_t>(other_x), static_cast<std::uint16_t>(other_y)) * 4U +
        static_cast<std::size_t>(opposite(edge.side))) = kind;
}

void generate_site_fortifications(SiteProceduralLayer& layer, const SiteFastVars& fast,
                                  const rules::Ruleset& ruleset) {
    // M3.0 裁定沒有 crossing 就不補假門；因此零主幹道時整座城不生成牆。
    if (fast.defense == 0 || layer.skeleton.gates.empty()) {
        return;
    }
    const auto ranked = important_blocks(layer);
    if (ranked.empty()) {
        return;
    }
    const auto scaled_defense = std::min<std::uint32_t>(fast.defense, 100U);
    const auto protected_count = std::max<std::size_t>(
        1U, (ranked.size() * static_cast<std::size_t>(scaled_defense) + 99U) / 100U);
    const auto outer = bounds_for(layer, ranked, protected_count);
    add_ring(layer, outer, fast, ruleset);

    const auto& config = ruleset.site_fill_rules().fortification;
    if (fast.defense >= config.double_wall_defense) {
        auto inner = bounds_for(layer, ranked, std::max<std::size_t>(1U, protected_count / 4U));
        if (inner == outer && inner.x0 + 2U < inner.x1 && inner.y0 + 2U < inner.y1) {
            ++inner.x0;
            ++inner.y0;
            --inner.x1;
            --inner.y1;
        }
        add_ring(layer, inner, fast, ruleset);
    }
    if (fast.defense >= config.moat_defense) {
        add_moat(layer, outer, ruleset);
    }
}

}  // namespace aetheria::site::fill_detail
