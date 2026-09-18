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

namespace aetheria::site {

void evict_frozen_site_zone(zone::ZoneManager& manager, zone::ZoneHandle handle,
                            world::RegionTiles& region_tiles,
                            world::RegionXY coordinate, time::Tick now) {
    const auto region_index = region_tiles.index_of(coordinate);
    const auto& site_state = region_tiles.site.at(region_index);
    if (site_state.has_live_site || site_state.lod != zone::LodLevel::Frozen) {
        throw std::logic_error{"Site evict 要求起點為 L_FROZEN"};
    }
    const bool borrowed = manager.with(handle, [](const zone::Zone& loaded) {
        if (loaded.lod != zone::LodLevel::Frozen ||
            loaded.reg.view<const SiteDigest>().size() != 1U) {
            throw std::logic_error{"Site evict 的記憶體 zone 不是單一 digest 的 L_FROZEN"};
        }
    });
    if (!borrowed) {
        throw std::logic_error{"Site evict 要求 zone 仍在記憶體"};
    }
    manager.tick(now);
    if (!manager.unload(handle.key())) {
        throw std::logic_error{"Site evict 無法寫盤並進入 L_ABSENT"};
    }
    auto& mutable_state = region_tiles.site.at(region_index);
    mutable_state.lod = zone::LodLevel::Absent;
    mutable_state.has_live_site = false;
}

void unload_site_zone(zone::ZoneManager& manager, zone::ZoneHandle handle,
                      world::RegionTiles& region_tiles, world::RegionXY coordinate,
                      std::uint64_t world_seed, std::uint32_t region_id, time::Tick now,
                      const rules::Ruleset& ruleset) {
    freeze_site_zone(manager, handle, region_tiles, coordinate, world_seed, region_id, now,
                     ruleset);
    evict_frozen_site_zone(manager, handle, region_tiles, coordinate, now);
}

void collapse_site_zone(zone::ZoneManager& manager, zone::ZoneHandle handle,
                        world::RegionTiles& region_tiles, world::RegionXY coordinate,
                        const rules::Ruleset& ruleset) {
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
        reduce_live_site_xun(region_tiles, coordinate, loaded, ruleset);
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
