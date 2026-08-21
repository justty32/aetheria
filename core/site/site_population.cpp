#include "core/site/site_projection.h"

#include "core/site/site_fill_detail.h"

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace aetheria::site {
namespace {

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
}

template <typename Value> void hash_integer(std::uint64_t& hash, Value value) noexcept {
    const auto bits = static_cast<std::make_unsigned_t<Value>>(value);
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits >> (byte * 8U)));
    }
}

}  // namespace

bool SiteProceduralLayer::valid_layout() const noexcept {
    if (!skeleton.valid_layout() || edges.size() != kSiteTileCount * 4U ||
        zoning.size() != kSiteTileCount || block_zoning.size() != skeleton.blocks.size() ||
        std::ranges::any_of(zoning,
                            [](SiteZoning zone) { return zone > SiteZoning::Commercial; }) ||
        std::ranges::any_of(block_zoning,
                            [](SiteZoning zone) { return zone > SiteZoning::Commercial; })) {
        return false;
    }
    return std::ranges::all_of(buildings, [&](const ProceduralBuilding& building) {
        if (building.width == 0 || building.height == 0 ||
            building.frontage > SiteBoundarySide::West ||
            building.damage > ProceduralBuildingDamage::Burned ||
            static_cast<std::uint32_t>(building.origin.x) + building.width > kSiteWidth ||
            static_cast<std::uint32_t>(building.origin.y) + building.height > kSiteHeight) {
            return false;
        }
        for (std::uint16_t y = building.origin.y; y < building.origin.y + building.height; ++y) {
            for (std::uint16_t x = building.origin.x; x < building.origin.x + building.width; ++x) {
                if (skeleton.buildable[fill_detail::tile_index(x, y)] == 0) {
                    return false;
                }
            }
        }
        return true;
    });
}

SiteProceduralLayer populate(SiteSkeleton skeleton, const SiteFastVars& fast,
                             const rules::Ruleset& ruleset) {
    if (!skeleton.valid_layout()) {
        throw std::runtime_error{"無法填充版面無效的 SiteSkeleton"};
    }
    if (fast.settlement > world::SettlementTier::City) {
        throw std::runtime_error{"SiteFastVars 含無效 SettlementTier"};
    }
    if (fast.damage > 100U) {
        throw std::runtime_error{"SiteFastVars 損毀度超過 100"};
    }
    if (!ruleset.site_fill_rules().loaded) {
        throw std::runtime_error{"Site 填充缺少 city fill 資料規則"};
    }

    auto fill_edges = skeleton.edges;
    SiteProceduralLayer result{std::move(skeleton),
                               std::move(fill_edges),
                               std::vector<SiteZoning>(kSiteTileCount, SiteZoning::Open),
                               {},
                               {},
                               {},
                               {},
                               {},
                               0};
    result.block_zoning.resize(result.skeleton.blocks.size(), SiteZoning::Open);
    if (fast.settlement != world::SettlementTier::None) {
        fill_detail::assign_site_zones(result, fast, ruleset);
        fill_detail::fill_site_buildings(result, fast, ruleset);
        fill_detail::generate_site_fortifications(result, fast, ruleset);
        fill_detail::place_site_landmark(result, fast, ruleset);
        fill_detail::apply_site_damage(result, fast, ruleset);
        const bool has_wall = std::ranges::any_of(result.edges, [&](rules::EdgeId edge) {
            const auto* definition = ruleset.edge(edge);
            return definition != nullptr && (definition->flags & rules::kEdgeWallFlag) != 0;
        });
        const bool has_openable_gate = std::ranges::any_of(result.edges, [&](rules::EdgeId edge) {
            const auto* definition = ruleset.edge(edge);
            constexpr auto required = rules::kEdgeGateFlag | rules::kEdgeOpenableFlag;
            return definition != nullptr && (definition->flags & required) == required;
        });
        if (has_wall && !has_openable_gate) {
            throw std::logic_error{"Site 填充違反不變式：有城牆但沒有可開關城門"};
        }
    }
    return result;
}

std::uint64_t hash_site_fill(const SiteProceduralLayer& procedural) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_integer(hash, static_cast<std::uint64_t>(procedural.edges.size()));
    for (const auto edge : procedural.edges) {
        hash_integer(hash, rules::value_of(edge));
    }
    hash_integer(hash, static_cast<std::uint64_t>(procedural.block_zoning.size()));
    for (const auto zone : procedural.block_zoning) {
        hash_byte(hash, static_cast<std::uint8_t>(zone));
    }
    hash_integer(hash, static_cast<std::uint64_t>(procedural.zoning.size()));
    for (const auto zone : procedural.zoning) {
        hash_byte(hash, static_cast<std::uint8_t>(zone));
    }
    hash_integer(hash, static_cast<std::uint64_t>(procedural.buildings.size()));
    for (const auto& building : procedural.buildings) {
        hash_integer(hash, rules::value_of(building.def));
        hash_integer(hash, building.origin.x);
        hash_integer(hash, building.origin.y);
        hash_byte(hash, building.width);
        hash_byte(hash, building.height);
        hash_byte(hash, static_cast<std::uint8_t>(building.frontage));
        hash_byte(hash, static_cast<std::uint8_t>(building.damage));
    }
    hash_byte(hash, procedural.wall_ring_count);
    for (const auto& edges :
         {&procedural.wall_edges, &procedural.wall_gates, &procedural.wall_breaches}) {
        hash_integer(hash, static_cast<std::uint64_t>(edges->size()));
        for (const auto& edge : *edges) {
            hash_integer(hash, edge.tile.x);
            hash_integer(hash, edge.tile.y);
            hash_byte(hash, static_cast<std::uint8_t>(edge.side));
        }
    }
    return hash;
}

bool valid_persistent_layer(const SitePersistentLayer& layer) noexcept {
    return std::ranges::all_of(layer.buildings, [](const PersistentBuilding& building) {
        return building.tile.x < kSiteWidth && building.tile.y < kSiteHeight &&
               building.type <= BuildingType::SettlementHall &&
               building.state <= BuildingState::Ruined;
    });
}

}  // namespace aetheria::site
