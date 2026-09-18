#include "core/site/site_fortifications_detail.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace aetheria::site::fill_detail {
using fortifications_detail::Bounds;
using fortifications_detail::add_moat;
using fortifications_detail::add_ring;
using fortifications_detail::offset_for;

namespace {

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
