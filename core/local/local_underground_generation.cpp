// local_underground_generation.cpp：路線 C 地表、負 z 層與相鄰層連結編排。

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

#include "core/local/local_underground.h"
#include "core/local/local_underground_detail.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::local {

bool UndergroundLocalSkeleton::valid_layout() const noexcept {
    if (kind == rules::UndergroundKind::None || depth == 0 ||
        layers.size() != static_cast<std::size_t>(depth) + 1U || !layers.contains(0) ||
        excavated.size() != depth || vertical_links.size() != depth) {
        return false;
    }
    if (!std::ranges::all_of(layers,
                             [](const auto& layer) { return layer.second.valid_layout(); })) {
        return false;
    }
    for (std::uint8_t level = 1; level <= depth; ++level) {
        const auto z = static_cast<std::int8_t>(-static_cast<std::int16_t>(level));
        const auto found = excavated.find(z);
        if (!layers.contains(z) || found == excavated.end() ||
            found->second.size() != kLocalTileCount ||
            std::ranges::none_of(found->second, [](std::uint8_t value) { return value != 0; })) {
            return false;
        }
    }
    return true;
}

UndergroundLocalSkeleton build_underground_local_skeleton(const LocalSlowVars& slow,
                                                          std::uint64_t local_seed,
                                                          const rules::Ruleset& ruleset) {
    if (!slow.structure.has_value()) {
        throw std::invalid_argument{"路線 C 要求 Site structure"};
    }
    const auto* structure = ruleset.building(*slow.structure);
    if (structure == nullptr || structure->underground == rules::UndergroundKind::None ||
        structure->underground_depth == 0) {
        throw std::invalid_argument{"路線 C 的 Site structure 未指定地下種類與深度"};
    }
    if (ruleset.ground(slow.ground) == nullptr || ruleset.feature(slow.feature) == nullptr ||
        !ruleset.local_building_rules().loaded) {
        throw std::runtime_error{"Local 路線 C 含無效慢變數或缺少資料規則"};
    }
    const auto none = ruleset.find_edge("edge.none");
    if (!none.has_value()) {
        throw std::runtime_error{"Local 路線 C 缺少 edge.none"};
    }
    const auto& building_rules = ruleset.local_building_rules();
    UndergroundLocalSkeleton result;
    result.kind = structure->underground;
    result.depth = structure->underground_depth;
    result.entrance = {kLocalWidth / 2U, kLocalHeight / 2U};
    auto surface_slow = slow;
    surface_slow.structure.reset();
    auto surface = build_open_local_skeleton(surface_slow, local_seed, ruleset);
    surface.tiles.overlay[underground_detail::tile_index(result.entrance)] = OverlayId::Stairs;
    result.layers.emplace(0, std::move(surface.tiles));

    auto incoming = result.entrance;
    for (std::uint8_t level = 1; level <= result.depth; ++level) {
        const auto z = static_cast<std::int8_t>(-static_cast<std::int16_t>(level));
        const auto seed =
            worldgen::splitmix64(local_seed ^ (static_cast<std::uint64_t>(level) << 48U));
        underground_detail::LayerBuild layer;
        switch (result.kind) {
            case rules::UndergroundKind::Mine:
                layer = underground_detail::build_mine_layer(incoming, z, seed,
                                                             building_rules.foundation_ground,
                                                             building_rules.wall_edge, *none);
                break;
            case rules::UndergroundKind::Dungeon:
                layer = underground_detail::build_dungeon_layer(incoming, z, seed,
                                                                building_rules.foundation_ground,
                                                                building_rules.wall_edge, *none);
                break;
            case rules::UndergroundKind::Ruin:
                layer = underground_detail::build_ruin_layer(slow, incoming, z, seed, ruleset,
                                                             result.ruin_original_segments,
                                                             result.ruin_removed_segments);
                break;
            case rules::UndergroundKind::None:
                throw std::logic_error{"路線 C 種類在生成中遺失"};
        }
        layer.tiles.overlay[underground_detail::tile_index(incoming)] = OverlayId::Stairs;
        const auto upper_z = static_cast<std::int8_t>(z + 1);
        result.layers.at(upper_z).overlay[underground_detail::tile_index(incoming)] =
            OverlayId::Stairs;
        result.vertical_links.push_back({incoming, upper_z, z});

        const auto room_base = static_cast<std::uint16_t>(result.rooms.size());
        for (auto corridor : layer.corridors) {
            if (corridor.from_room != std::numeric_limits<std::uint16_t>::max()) {
                corridor.from_room = static_cast<std::uint16_t>(corridor.from_room + room_base);
                corridor.to_room = static_cast<std::uint16_t>(corridor.to_room + room_base);
            }
            result.corridors.push_back(corridor);
        }
        result.rooms.insert(result.rooms.end(), layer.rooms.begin(), layer.rooms.end());
        result.excavated_count +=
            static_cast<std::uint32_t>(std::ranges::count(layer.excavated, 1));
        result.excavated.emplace(z, std::move(layer.excavated));
        incoming = layer.exit;
        result.layers.emplace(z, std::move(layer.tiles));
    }
    if (!valid_underground_invariants(result, ruleset)) {
        throw std::logic_error{"Local 路線 C 產生不可達或垂直連結不一致的地下結構"};
    }
    return result;
}

}  // namespace aetheria::local
