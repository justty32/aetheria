#include "core/site/site_materialize.h"

#include "core/site/site_projection.h"
#include "core/site/site_lifecycle.h"
#include "core/site/site_reduction.h"
#include "core/site/site_wilderness.h"
#include "core/zone/zone_key.h"

#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace aetheria::site {
namespace {

[[nodiscard]] zone::ZoneKey site_key_for(std::uint32_t region_id,
                                         world::RegionXY coordinate) {
    if (region_id > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error{"Site materialize 的 region_id 超過 ZoneKey 容量"};
    }
    if (coordinate.x < 0 || coordinate.y < 0 || coordinate.x > 0x0FFF ||
        coordinate.y > 0x0FFF) {
        throw std::runtime_error{"Site materialize 的座標超過 ZoneKey 容量"};
    }
    const auto region_key = zone::child_key(zone::kRootZone, region_id, 0);
    return zone::child_key(region_key, static_cast<std::uint16_t>(coordinate.x),
                           static_cast<std::uint16_t>(coordinate.y));
}

[[nodiscard]] SiteXY initial_building_tile(const SiteProceduralLayer& procedural) {
    for (std::size_t index = 0; index < kSiteTileCount; ++index) {
        if (procedural.zoning[index] != SiteZoning::Open &&
            procedural.skeleton.buildable[index] != 0) {
            return {static_cast<std::uint16_t>(index % kSiteWidth),
                    static_cast<std::uint16_t>(index / kSiteWidth)};
        }
    }
    for (std::size_t index = 0; index < kSiteTileCount; ++index) {
        if (procedural.skeleton.buildable[index] != 0) {
            return {static_cast<std::uint16_t>(index % kSiteWidth),
                    static_cast<std::uint16_t>(index / kSiteWidth)};
        }
    }
    throw std::runtime_error{"聚落 Site 沒有可放置持久建築的可建地"};
}

[[nodiscard]] SiteProceduralLayer wilderness_procedural_layer(SiteSkeleton skeleton) {
    auto edges = skeleton.edges;
    std::vector<SiteZoning> zoning(kSiteTileCount, SiteZoning::Open);
    std::vector<SiteZoning> block_zoning(skeleton.blocks.size(), SiteZoning::Open);
    return {std::move(skeleton), std::move(edges), std::move(zoning), std::move(block_zoning),
            {}, {}, {}, {}, 0};
}

struct PreparedSite {
    SiteProceduralLayer procedural;
    std::optional<WildernessSite> wilderness;
};

[[nodiscard]] PreparedSite prepare_site(const world::RegionTiles& region_tiles,
                                        world::RegionXY coordinate, std::uint64_t world_seed,
                                        std::uint32_t region_id, const SiteProjectionVars& vars,
                                        const rules::Ruleset& ruleset) {
    const auto seed = derive_site_seed(world_seed, region_id,
                                       static_cast<std::uint16_t>(coordinate.x),
                                       static_cast<std::uint16_t>(coordinate.y));
    if (vars.fast.settlement != world::SettlementTier::None) {
        return {populate(build_site_skeleton(vars.slow, seed, ruleset), vars.fast, ruleset),
                std::nullopt};
    }
    auto wilderness =
        generate_wilderness_site(region_tiles, coordinate, world_seed, region_id, ruleset);
    auto procedural = wilderness_procedural_layer(std::move(wilderness.skeleton.terrain));
    return {std::move(procedural), std::move(wilderness)};
}

}  // namespace

zone::Zone materialize_site_zone(world::RegionTiles& region_tiles, world::RegionXY coordinate,
                                 std::uint64_t world_seed, std::uint32_t region_id,
                                 const rules::Ruleset& ruleset) {
    const auto region_index = region_tiles.index_of(coordinate);
    if (region_tiles.site.at(region_index).has_live_site) {
        throw std::logic_error{"Site materialize 拒絕重複的 live Site"};
    }
    const auto site_key = site_key_for(region_id, coordinate);
    const auto vars = split_site_vars(region_tiles, coordinate);
    auto prepared = prepare_site(region_tiles, coordinate, world_seed, region_id, vars, ruleset);

    SitePersistentLayer persistent;
    if (vars.fast.settlement != world::SettlementTier::None) {
        persistent.buildings.push_back({initial_building_tile(prepared.procedural),
                                        BuildingType::SettlementHall, BuildingState::Active});
    }

    zone::SitePayload payload{
        SiteLayers{std::move(prepared.procedural), std::move(persistent), SiteVolatileLayer{}}};
    zone::Zone result{site_key, std::move(payload)};
    if (prepared.wilderness.has_value()) {
        install_wilderness_entities(result, *prepared.wilderness, vars.slow.feature,
                                    vars.fast.owner);
    }
    result.lod = zone::LodLevel::Coarse;
    auto& site_state = region_tiles.site.at(region_index);
    site_state.lod = zone::LodLevel::Coarse;
    site_state.has_live_site = true;
    site_state.ever_realized = true;
    return result;
}

zone::ZoneHandle rematerialize_site_zone(zone::ZoneManager& manager,
                                         world::RegionTiles& region_tiles,
                                         world::RegionXY coordinate, std::uint64_t world_seed,
                                         std::uint32_t region_id, const rules::Ruleset& ruleset) {
    return rematerialize_site_zone(manager, region_tiles, coordinate, world_seed, region_id,
                                   time::Tick{}, ruleset, nullptr);
}

zone::ZoneHandle rematerialize_site_zone(zone::ZoneManager& manager,
                                         world::RegionTiles& region_tiles,
                                         world::RegionXY coordinate, std::uint64_t world_seed,
                                         std::uint32_t region_id, time::Tick now,
                                         const rules::Ruleset& ruleset,
                                         SiteCatchUpReport* report) {
    const auto site_key = site_key_for(region_id, coordinate);
    if (manager.get(site_key).has_value()) {
        throw std::logic_error{"Site rematerialize 要求起點為 L_ABSENT（未載入）"};
    }
    const auto region_index = region_tiles.index_of(coordinate);
    if (region_tiles.site.at(region_index).has_live_site) {
        throw std::logic_error{"Site rematerialize 拒絕重複的 live Site"};
    }

    const auto vars = split_site_vars(region_tiles, coordinate);
    auto prepared = prepare_site(region_tiles, coordinate, world_seed, region_id, vars, ruleset);
    if (!manager.load(site_key)) {
        throw std::runtime_error{"Site rematerialize 找不到磁碟持久層"};
    }

    const auto handle = *manager.get(site_key);
    const bool borrowed = manager.with(handle, [&](zone::Zone& loaded) {
        auto& layers = std::get<zone::SitePayload>(loaded.payload).layers;
        layers.procedural = std::move(prepared.procedural);
        layers.volatile_state = SiteVolatileLayer{};
        if (prepared.wilderness.has_value()) {
            install_wilderness_entities(loaded, *prepared.wilderness, vars.slow.feature,
                                        vars.fast.owner);
        }
        auto digests = loaded.reg.view<SiteDigest>();
        if (!digests.empty()) {
            if (digests.size() != 1U) {
                throw std::runtime_error{"重載 Site 含多份 SiteDigest"};
            }
            auto& digest = digests.get<SiteDigest>(*digests.begin());
            const auto expected_seed =
                derive_site_seed(world_seed, region_id,
                                 static_cast<std::uint16_t>(coordinate.x),
                                 static_cast<std::uint16_t>(coordinate.y));
            const auto rebuilt_hash = hash_site_skeleton(layers.procedural.skeleton);
            if (digest.site_seed != expected_seed) {
                throw std::runtime_error{
                    "SiteDigest seed 不符（" + std::to_string(digest.site_seed) + "/" +
                    std::to_string(expected_seed) + ")"};
            }
            SiteMigrationReport migration;
            if (digest.skeleton_hash != rebuilt_hash) {
                migration = migrate_site_digest(digest, layers.procedural.skeleton, ruleset);
            }
            auto caught_up = restore_site_digest(loaded, vars.fast, now, ruleset);
            caught_up.migration = migration;
            if (report != nullptr) {
                *report = caught_up;
            }
        } else if (report != nullptr) {
            *report = {};
        }
        loaded.lod = zone::LodLevel::Coarse;
    });
    if (!borrowed) {
        throw std::logic_error{"Site rematerialize 冷載後未留在 ZoneManager"};
    }
    auto& site_state = region_tiles.site.at(region_index);
    site_state.lod = zone::LodLevel::Coarse;
    site_state.has_live_site = true;
    site_state.ever_realized = true;
    return handle;
}

void unload_site_zone(zone::ZoneManager& manager, zone::ZoneHandle handle,
                      world::RegionTiles& region_tiles, world::RegionXY coordinate,
                      std::uint64_t world_seed, std::uint32_t region_id, time::Tick now) {
    if (zone::level_of(handle.key()) != zone::ZoneLevel::Site) {
        throw std::invalid_argument{"Site unload 只接受 Site ZoneKey"};
    }
    const auto region_index = region_tiles.index_of(coordinate);
    if (!region_tiles.site.at(region_index).has_live_site) {
        throw std::logic_error{"Site unload 要求 Region tile 標記為 live"};
    }
    const bool borrowed = manager.with(handle, [&](zone::Zone& loaded) {
        if (loaded.lod != zone::LodLevel::Full && loaded.lod != zone::LodLevel::Coarse) {
            throw std::logic_error{"Site unload 要求起點為 L_FULL/L_COARSE"};
        }
        reduce_live_site_xun(region_tiles, coordinate, loaded);
        auto states = loaded.reg.view<CityBuildState>();
        if (states.size() != 1U) {
            throw std::logic_error{"M4 Site unload 要求恰有一個 CityBuildState"};
        }
        const auto state_entity = *states.begin();
        const auto& state = states.get<CityBuildState>(state_entity);
        auto& layers = std::get<zone::SitePayload>(loaded.payload).layers;
        if (!layers.procedural.skeleton.valid_layout()) {
            throw std::logic_error{"Site unload 不能從只有持久層的冷 load 建立 digest"};
        }
        SiteDigest digest{
            .unload_tick = now,
            .site_seed = derive_site_seed(world_seed, region_id,
                                          static_cast<std::uint16_t>(coordinate.x),
                                          static_cast<std::uint16_t>(coordinate.y)),
            .skeleton_hash = hash_site_skeleton(layers.procedural.skeleton),
            .objects = layers.persistent.buildings,
            .city_buildings = state.buildings,
            .pending = state.pending,
            .economy = state.economy,
            .story_flags = {},
            .migration = state.migration,
        };
        loaded.reg.emplace_or_replace<SiteDigest>(state_entity, std::move(digest));
        loaded.reg.remove<CityBuildState>(state_entity);
        layers.persistent.buildings.clear();
        loaded.lod = zone::LodLevel::Absent;
    });
    if (!borrowed) {
        throw std::logic_error{"Site unload 要求 zone 已載入"};
    }
    manager.tick(now);
    if (!manager.unload(handle.key())) {
        throw std::logic_error{"Site unload 無法寫盤並進入 L_ABSENT"};
    }
    auto& site_state = region_tiles.site.at(region_index);
    site_state.lod = zone::LodLevel::Absent;
    site_state.has_live_site = false;
}

void collapse_site_zone(zone::ZoneManager& manager, zone::ZoneHandle handle,
                        world::RegionTiles& region_tiles, world::RegionXY coordinate) {
    if (zone::level_of(handle.key()) != zone::ZoneLevel::Site) {
        throw std::invalid_argument{"Site collapse 只接受 Site ZoneKey"};
    }
    const auto region_index = region_tiles.index_of(coordinate);
    if (!region_tiles.site.at(region_index).has_live_site) {
        throw std::logic_error{"Site collapse 要求 Region tile 標記為 live"};
    }
    const bool borrowed = manager.with(handle, [&](const zone::Zone& loaded) {
        if (loaded.lod != zone::LodLevel::Coarse) {
            throw std::logic_error{"Site collapse 要求起點為 L_COARSE"};
        }
        reduce_live_site_xun(region_tiles, coordinate, loaded);
    });
    if (!borrowed) {
        throw std::logic_error{"Site collapse 要求 zone 已載入"};
    }
    if (!manager.unload(handle.key())) {
        throw std::logic_error{"Site collapse 無法寫盤並進入 L_ABSENT"};
    }
    auto& site_state = region_tiles.site.at(region_index);
    site_state.lod = zone::LodLevel::Absent;
    site_state.has_live_site = false;
}

}  // namespace aetheria::site
