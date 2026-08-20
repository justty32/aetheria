#include "core/worldgen/influence_spread.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <unordered_set>

namespace aetheria::worldgen {
namespace {

[[nodiscard]] std::uint32_t manhattan(world::RegionXY lhs, world::RegionXY rhs) noexcept {
    const auto dx = std::abs(static_cast<int>(lhs.x) - static_cast<int>(rhs.x));
    const auto dy = std::abs(static_cast<int>(lhs.y) - static_cast<int>(rhs.y));
    return static_cast<std::uint32_t>(dx + dy);
}

}  // namespace

std::vector<CitySite> select_capitals(std::span<const CitySite> cities,
                                      std::size_t faction_count) {
    std::vector<CitySite> eligible;
    eligible.reserve(cities.size());
    std::unordered_set<std::uint32_t> canonical_ids;
    for (const auto& city : cities) {
        if (city.tier == world::SettlementTier::City) {
            if (!canonical_ids.insert(city.canonical_id).second) {
                throw std::invalid_argument{"大城 canonical id 重複"};
            }
            eligible.push_back(city);
        }
    }
    if (faction_count > eligible.size()) {
        throw std::invalid_argument{"勢力數超過可用大城數"};
    }
    std::vector<CitySite> selected;
    selected.reserve(faction_count);
    while (selected.size() < faction_count) {
        const auto candidate = std::ranges::max_element(eligible, [&](const CitySite& lhs,
                                                                      const CitySite& rhs) {
            const auto minimum_distance = [&](const CitySite& site) {
                if (selected.empty()) {
                    return std::numeric_limits<std::uint32_t>::max();
                }
                return std::ranges::min(selected | std::views::transform([&](const CitySite& other) {
                                            return manhattan(site.tile, other.tile);
                                        }));
            };
            return std::tuple{minimum_distance(lhs), lhs.score,
                              std::numeric_limits<std::uint32_t>::max() - lhs.canonical_id} <
                   std::tuple{minimum_distance(rhs), rhs.score,
                              std::numeric_limits<std::uint32_t>::max() - rhs.canonical_id};
        });
        selected.push_back(*candidate);
        eligible.erase(candidate);
    }
    return selected;
}

}  // namespace aetheria::worldgen
