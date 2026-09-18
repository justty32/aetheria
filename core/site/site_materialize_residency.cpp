#include "core/site/site_materialize.h"

#include "core/site/site_lifecycle.h"
#include "core/site/site_projection.h"
#include "core/site/site_reduction.h"
#include "core/site/site_wilderness.h"
#include "core/zone/zone_key.h"

#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "core/site/site_materialize_detail.h"

namespace aetheria::site {
namespace {

void clear_wilderness_entities(zone::Zone& loaded) {
    std::vector<entt::entity> entities;
    for (const auto entity : loaded.reg.view<SitePosition>()) {
        entities.push_back(entity);
    }
    for (const auto entity : entities) {
        loaded.reg.destroy(entity);
    }
}

}  // namespace

using detail::prepare_site;
using detail::restore_loaded_site;

void freeze_site_zone(zone::ZoneManager& manager, zone::ZoneHandle handle,
                      world::RegionTiles& region_tiles, world::RegionXY coordinate,
                      std::uint64_t world_seed, std::uint32_t region_id, time::Tick now,
                      const rules::Ruleset& ruleset) {
    if (zone::level_of(handle.key()) != zone::ZoneLevel::Site) {
        throw std::invalid_argument{"Site freeze 只接受 Site ZoneKey"};
    }
    const auto region_index = region_tiles.index_of(coordinate);
    if (!region_tiles.site.at(region_index).has_live_site) {
        throw std::logic_error{"Site freeze 要求 Region tile 標記為 live"};
    }
    const bool borrowed = manager.with(handle, [&](zone::Zone& loaded) {
        if (loaded.lod != zone::LodLevel::Full && loaded.lod != zone::LodLevel::Coarse) {
            throw std::logic_error{"Site freeze 要求起點為 L_FULL/L_COARSE"};
        }
        reduce_live_site_xun(region_tiles, coordinate, loaded, ruleset);
        auto states = loaded.reg.view<CityBuildState>();
        if (states.size() != 1U) {
            throw std::logic_error{"Site freeze 要求恰有一個 CityBuildState"};
        }
        const auto state_entity = *states.begin();
        const auto& state = states.get<CityBuildState>(state_entity);
        auto& layers = std::get<zone::SitePayload>(loaded.payload).layers;
        if (!layers.procedural.skeleton.valid_layout()) {
            throw std::logic_error{"Site freeze 不能從只有持久層的冷 load 建立 digest"};
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
        clear_wilderness_entities(loaded);
        layers.persistent.buildings.clear();
        layers.procedural = {};
        layers.volatile_state = {};
        loaded.lod = zone::LodLevel::Frozen;
    });
    if (!borrowed) {
        throw std::logic_error{"Site freeze 要求 zone 已載入"};
    }
    auto& site_state = region_tiles.site.at(region_index);
    site_state.lod = zone::LodLevel::Frozen;
    site_state.has_live_site = false;
}

void thaw_site_zone(zone::ZoneManager& manager, zone::ZoneHandle handle,
                    world::RegionTiles& region_tiles, world::RegionXY coordinate,
                    std::uint64_t world_seed, std::uint32_t region_id, time::Tick now,
                    const rules::Ruleset& ruleset, SiteCatchUpReport* report) {
    if (zone::level_of(handle.key()) != zone::ZoneLevel::Site) {
        throw std::invalid_argument{"Site thaw 只接受 Site ZoneKey"};
    }
    const auto region_index = region_tiles.index_of(coordinate);
    const auto& site_state = region_tiles.site.at(region_index);
    if (site_state.has_live_site || site_state.lod != zone::LodLevel::Frozen) {
        throw std::logic_error{"Site thaw 要求起點為 L_FROZEN"};
    }
    const auto vars = split_site_vars(region_tiles, coordinate);
    auto prepared = prepare_site(region_tiles, coordinate, world_seed, region_id, vars, ruleset);
    const auto expected_seed =
        derive_site_seed(world_seed, region_id, static_cast<std::uint16_t>(coordinate.x),
                         static_cast<std::uint16_t>(coordinate.y));
    const bool borrowed = manager.with(handle, [&](zone::Zone& loaded) {
        if (loaded.lod != zone::LodLevel::Frozen || loaded.reg.view<SiteDigest>().size() != 1U) {
            throw std::logic_error{"Site thaw 的記憶體 zone 不是單一 digest 的 L_FROZEN"};
        }
        restore_loaded_site(loaded, std::move(prepared), vars, expected_seed, now, ruleset,
                            report);
    });
    if (!borrowed) {
        throw std::logic_error{"Site thaw 要求 zone 仍在記憶體"};
    }
    auto& mutable_state = region_tiles.site.at(region_index);
    mutable_state.lod = zone::LodLevel::Coarse;
    mutable_state.has_live_site = true;
    mutable_state.ever_realized = true;
}

}  // namespace aetheria::site
