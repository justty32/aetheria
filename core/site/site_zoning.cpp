#include "core/site/site_fill_detail.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

namespace aetheria::site::fill_detail {
namespace {

[[nodiscard]] std::uint32_t buildable_area(const SiteSkeleton& skeleton,
                                           const SiteBlock& block) {
    std::uint32_t result{};
    for (std::uint16_t y = block.origin.y; y < block.origin.y + block.height; ++y) {
        for (std::uint16_t x = block.origin.x; x < block.origin.x + block.width; ++x) {
            result += skeleton.buildable[tile_index(x, y)] != 0 ? 1U : 0U;
        }
    }
    return result;
}

[[nodiscard]] std::uint32_t road_frontage(const SiteSkeleton& skeleton,
                                          const SiteBlock& block) {
    std::uint32_t result{};
    const auto x_end = static_cast<std::uint16_t>(block.origin.x + block.width);
    const auto y_end = static_cast<std::uint16_t>(block.origin.y + block.height);
    for (std::uint16_t x = block.origin.x; x < x_end; ++x) {
        result += block.origin.y > 0 && skeleton.roads[tile_index(x, block.origin.y - 1U)] != 0;
        result += y_end < kSiteHeight && skeleton.roads[tile_index(x, y_end)] != 0;
    }
    for (std::uint16_t y = block.origin.y; y < y_end; ++y) {
        result += block.origin.x > 0 && skeleton.roads[tile_index(block.origin.x - 1U, y)] != 0;
        result += x_end < kSiteWidth && skeleton.roads[tile_index(x_end, y)] != 0;
    }
    return result;
}

[[nodiscard]] std::uint32_t block_score(const SiteSkeleton& skeleton, std::size_t block_index,
                                        rules::SiteFillZone zone) {
    const auto& block = skeleton.blocks[block_index];
    const auto center_x = static_cast<std::uint32_t>(block.origin.x) + block.width / 2U;
    const auto center_y = static_cast<std::uint32_t>(block.origin.y) + block.height / 2U;
    const auto distance = static_cast<std::uint32_t>(
        std::abs(static_cast<int>(center_x) - static_cast<int>(skeleton.city_center.x)) +
        std::abs(static_cast<int>(center_y) - static_cast<int>(skeleton.city_center.y)));
    const auto frontage = road_frontage(skeleton, block);
    if (zone == rules::SiteFillZone::Commercial) {
        return distance * 64U + (256U - std::min(frontage, UINT32_C(256)));
    }
    constexpr std::uint32_t kResidentialRing = 18;
    const auto ring_error = distance > kResidentialRing ? distance - kResidentialRing
                                                         : kResidentialRing - distance;
    return ring_error * 64U + (256U - std::min(frontage, UINT32_C(256)));
}

[[nodiscard]] std::uint64_t driver_value(const SiteFastVars& fast,
                                         rules::SiteQuotaDriver driver) noexcept {
    if (driver == rules::SiteQuotaDriver::Population) {
        return fast.population;
    }
    return fast.development_level;
}

}  // namespace

void assign_site_zones(SiteProceduralLayer& layer, const SiteFastVars& fast,
                       const rules::Ruleset& ruleset) {
    std::vector<std::size_t> eligible;
    for (std::size_t index = 0; index < layer.skeleton.blocks.size(); ++index) {
        if (buildable_area(layer.skeleton, layer.skeleton.blocks[index]) >= 4U) {
            eligible.push_back(index);
        }
    }

    for (const auto& quota : ruleset.site_fill_rules().quotas) {
        const auto value = driver_value(fast, quota.driver);
        if (value == 0 || eligible.empty()) {
            continue;
        }
        const auto raw = (value + quota.units_per_block - 1U) / quota.units_per_block;
        const auto cap = std::max<std::uint64_t>(
            1U, static_cast<std::uint64_t>(eligible.size()) * quota.max_percent / 100U);
        const auto target = static_cast<std::size_t>(std::min(raw, cap));

        std::vector<std::size_t> ranked = eligible;
        std::ranges::sort(ranked, [&](std::size_t left, std::size_t right) {
            const auto left_score = block_score(layer.skeleton, left, quota.zone);
            const auto right_score = block_score(layer.skeleton, right, quota.zone);
            return left_score != right_score ? left_score < right_score : left < right;
        });
        std::size_t assigned{};
        for (const auto block_index : ranked) {
            if (layer.block_zoning[block_index] != SiteZoning::Open) {
                continue;
            }
            layer.block_zoning[block_index] = zoning_for(quota.zone);
            if (++assigned == target) {
                break;
            }
        }
    }

    for (std::size_t block_index = 0; block_index < layer.skeleton.blocks.size(); ++block_index) {
        const auto zone = layer.block_zoning[block_index];
        if (zone == SiteZoning::Open) {
            continue;
        }
        const auto& block = layer.skeleton.blocks[block_index];
        for (std::uint16_t y = block.origin.y; y < block.origin.y + block.height; ++y) {
            for (std::uint16_t x = block.origin.x; x < block.origin.x + block.width; ++x) {
                const auto index = tile_index(x, y);
                if (layer.skeleton.buildable[index] != 0) {
                    layer.zoning[index] = zone;
                }
            }
        }
    }
}

}  // namespace aetheria::site::fill_detail
