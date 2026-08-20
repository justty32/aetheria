#include "core/worldgen/influence_spread.h"

#include "core/world/region_movement.h"

#include <algorithm>
#include <array>
#include <limits>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <unordered_set>

namespace aetheria::worldgen {
namespace {

[[nodiscard]] std::uint16_t faction_value(world::FactionId faction) noexcept {
    return static_cast<std::uint16_t>(faction);
}

struct InfluenceKey {
    std::int64_t cost{std::numeric_limits<std::int64_t>::max()};
    world::FactionId faction{static_cast<world::FactionId>(UINT16_MAX)};
    std::size_t capital_index{std::numeric_limits<std::size_t>::max()};

    [[nodiscard]] auto canonical() const noexcept {
        return std::tuple{cost, faction_value(faction), capital_index};
    }
};

struct QueueEntry {
    InfluenceKey key;
    std::size_t tile_index{};
};

struct QueueGreater {
    [[nodiscard]] bool operator()(const QueueEntry& lhs, const QueueEntry& rhs) const noexcept {
        return std::tuple{lhs.key.cost, faction_value(lhs.key.faction), lhs.key.capital_index,
                          lhs.tile_index} >
               std::tuple{rhs.key.cost, faction_value(rhs.key.faction), rhs.key.capital_index,
                          rhs.tile_index};
    }
};

[[nodiscard]] bool is_water(const world::RegionTiles& tiles, std::size_t index,
                            const rules::Ruleset& ruleset) {
    const auto* terrain = ruleset.terrain(tiles.base.at(index));
    if (terrain == nullptr) {
        throw std::runtime_error{"RegionTiles 含不存在的 TerrainId"};
    }
    return (terrain->flags & rules::kTerrainWaterFlag) != 0;
}

[[nodiscard]] world::RegionXY coordinate(std::size_t index, std::uint32_t width) noexcept {
    return {static_cast<std::int16_t>(index % width),
            static_cast<std::int16_t>(index / width)};
}

}  // namespace

std::vector<world::FactionId>
spread_influence(const world::RegionTiles& tiles, std::span<const InfluenceCapital> capitals,
                 const rules::Ruleset& ruleset,
                 const rules::CivilizationRules::FactionRules& factions,
                 InfluenceSpreadDiagnostics* diagnostics) {
    if (!tiles.valid_layout() ||
        tiles.width > static_cast<std::uint32_t>(std::numeric_limits<std::int16_t>::max()) ||
        tiles.height > static_cast<std::uint32_t>(std::numeric_limits<std::int16_t>::max())) {
        throw std::invalid_argument{"影響力擴散需要有效且可用 RegionXY 表示的 RegionTiles"};
    }
    if (factions.influence_max_cost < 0 || factions.influence_season < 1 ||
        factions.influence_season > 4) {
        throw std::invalid_argument{"影響力預算或季節無效"};
    }

    const auto count = tiles.tile_count();
    std::vector<InfluenceKey> best(count);
    std::vector<std::uint32_t> updates(count);
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueGreater> open;
    InfluenceSpreadDiagnostics measured;
    std::unordered_set<std::uint16_t> faction_ids;
    std::unordered_set<std::size_t> capital_tiles;
    for (const auto& capital : capitals) {
        const auto faction = faction_value(capital.faction);
        const auto index = tiles.index_of(capital.tile);
        if (faction == 0 || !faction_ids.insert(faction).second ||
            !capital_tiles.insert(index).second || is_water(tiles, index, ruleset)) {
            throw std::invalid_argument{"首都必須位於不同陸格並使用不同的非零勢力 id"};
        }
        const InfluenceKey candidate{0, capital.faction, index};
        if (candidate.canonical() < best[index].canonical()) {
            best[index] = candidate;
            open.push({candidate, index});
            ++measured.queue_pushes;
            updates[index] = 1;
        }
    }

    constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kDirections{{
        {0, -1},
        {1, 0},
        {0, 1},
        {-1, 0},
    }};
    while (!open.empty()) {
        const auto current = open.top();
        open.pop();
        if (current.key.canonical() != best[current.tile_index].canonical()) {
            ++measured.stale_pops;
            continue;
        }
        const auto here = coordinate(current.tile_index, tiles.width);
        for (const auto& [dx, dy] : kDirections) {
            const auto x = static_cast<std::int32_t>(here.x) + dx;
            const auto y = static_cast<std::int32_t>(here.y) + dy;
            if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(tiles.width) ||
                y >= static_cast<std::int32_t>(tiles.height)) {
                continue;
            }
            const world::RegionXY next{static_cast<std::int16_t>(x),
                                       static_cast<std::int16_t>(y)};
            const auto next_index = tiles.index_of(next);
            if (is_water(tiles, next_index, ruleset)) {
                continue;
            }
            const auto step = world::region_step_cost(tiles, here, next, ruleset,
                                                      factions.influence_season);
            if (current.key.cost > factions.influence_max_cost - step) {
                continue;
            }
            const InfluenceKey candidate{current.key.cost + step, current.key.faction,
                                         current.key.capital_index};
            if (candidate.canonical() < best[next_index].canonical()) {
                if (best[next_index].cost != std::numeric_limits<std::int64_t>::max() &&
                    candidate.cost == best[next_index].cost) {
                    ++measured.tie_relabels;
                }
                best[next_index] = candidate;
                open.push({candidate, next_index});
                ++measured.queue_pushes;
                ++updates[next_index];
            }
        }
    }

    std::vector<world::FactionId> owners(count, world::FactionId{0});
    for (std::size_t index = 0; index < count; ++index) {
        if (best[index].cost <= factions.influence_max_cost) {
            owners[index] = best[index].faction;
        }
    }
    measured.maximum_updates_per_tile =
        *std::max_element(updates.begin(), updates.end());
    if (diagnostics != nullptr) {
        *diagnostics = measured;
    }
    return owners;
}

}  // namespace aetheria::worldgen
