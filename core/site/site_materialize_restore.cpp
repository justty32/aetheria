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
namespace detail {

void restore_loaded_site(zone::Zone& loaded, PreparedSite prepared,
                         const SiteProjectionVars& vars, std::uint64_t expected_seed,
                         time::Tick now, const rules::Ruleset& ruleset,
                         SiteCatchUpReport* report) {
    auto& layers = std::get<zone::SitePayload>(loaded.payload).layers;
    layers.procedural = std::move(prepared.procedural);
    layers.volatile_state = SiteVolatileLayer{};
    if (prepared.wilderness.has_value()) {
        install_wilderness_entities(loaded, *prepared.wilderness, vars.slow.feature,
                                    vars.fast.owner);
    }
    auto digests = loaded.reg.view<SiteDigest>();
    if (digests.empty()) {
        if (report != nullptr) {
            *report = {};
        }
        loaded.lod = zone::LodLevel::Coarse;
        return;
    }
    if (digests.size() != 1U) {
        throw std::runtime_error{"重載 Site 含多份 SiteDigest"};
    }
    auto& digest = digests.get<SiteDigest>(*digests.begin());
    const auto rebuilt_hash = hash_site_skeleton(layers.procedural.skeleton);
    if (digest.site_seed != expected_seed) {
        throw std::runtime_error{"SiteDigest seed 不符（" + std::to_string(digest.site_seed) +
                                 "/" + std::to_string(expected_seed) + ")"};
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
    loaded.lod = zone::LodLevel::Coarse;
}

}  // namespace detail

using detail::prepare_site;
using detail::restore_loaded_site;
using detail::site_key_for;

zone::Zone rematerialize_site_zone(zone::Zone persistent,
                                   world::RegionTiles& region_tiles,
                                   world::RegionXY coordinate, std::uint64_t world_seed,
                                   std::uint32_t region_id, time::Tick now,
                                   const rules::Ruleset& ruleset,
                                   SiteCatchUpReport* report) {
    const auto site_key = site_key_for(region_id, coordinate);
    if (persistent.key != site_key) {
        throw std::invalid_argument{"Site rematerialize 持久快照的 ZoneKey 不符"};
    }
    const auto region_index = region_tiles.index_of(coordinate);
    if (region_tiles.site.at(region_index).has_live_site) {
        throw std::logic_error{"Site rematerialize 拒絕重複的 live Site"};
    }
    const auto vars = split_site_vars(region_tiles, coordinate);
    auto prepared = prepare_site(region_tiles, coordinate, world_seed, region_id, vars, ruleset);
    const auto expected_seed =
        derive_site_seed(world_seed, region_id,
                         static_cast<std::uint16_t>(coordinate.x),
                         static_cast<std::uint16_t>(coordinate.y));
    restore_loaded_site(persistent, std::move(prepared), vars, expected_seed, now, ruleset,
                        report);
    auto& site_state = region_tiles.site.at(region_index);
    site_state.lod = zone::LodLevel::Coarse;
    site_state.has_live_site = true;
    site_state.ever_realized = true;
    return persistent;
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
        const auto expected_seed =
            derive_site_seed(world_seed, region_id,
                             static_cast<std::uint16_t>(coordinate.x),
                             static_cast<std::uint16_t>(coordinate.y));
        restore_loaded_site(loaded, std::move(prepared), vars, expected_seed, now, ruleset,
                            report);
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

}  // namespace aetheria::site
