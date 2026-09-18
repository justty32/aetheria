#include "core/api/version.h"
#include "core/runtime/save_raws.h"
#include "core/rules/ruleset.h"
#include "core/serialize/zone_codec.h"
#include "core/site/site_materialize.h"
#include "core/time/tick.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/zone_manager.h"
#include "sim/gen_commands.h"
#include "sim/local_viewer.h"
#include "sim/site_viewer.h"
#include "sim/terrain_metrics.h"
#include "sim/world_hash.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "sim/core_probe_detail.h"


namespace aetheria::sim::detail {

int run_core_probe(const rules::Ruleset& ruleset, std::int64_t requested_tick,
                   const std::string& save_directory) {
    std::cout << "Aetheria core " << aetheria::core_version() << '\n';
    const std::array ticks{std::int64_t{0}, std::int64_t{864'000}, requested_tick};
    for (const auto raw_tick : ticks) {
        const auto date = aetheria::time::to_date(aetheria::time::Tick{raw_tick});
        std::cout << "tick=" << raw_tick << " -> year=" << date.year
                  << " season=" << static_cast<unsigned>(date.season)
                  << " month=" << static_cast<unsigned>(date.month)
                  << " xun=" << static_cast<unsigned>(date.xun) << '\n';
    }

    const auto region = aetheria::zone::child_key(aetheria::zone::kRootZone, 1, 0);
    const auto site = aetheria::zone::child_key(region, 4, 7);
    const auto local = aetheria::zone::child_key(site, 12, 9);

    const auto make_probe_region = [&]() {
        aetheria::world::RegionTiles tiles{8, 8};
        std::ranges::fill(tiles.base, *ruleset.find_terrain("terrain.grassland"));
        std::ranges::fill(tiles.relief, *ruleset.find_relief("relief.plain"));
        std::ranges::fill(tiles.feature, *ruleset.find_feature("feature.none"));
        std::ranges::fill(tiles.edges, *ruleset.find_edge("edge.none"));
        const auto index = tiles.index_of({4, 7});
        tiles.owner[index] = static_cast<aetheria::world::FactionId>(1);
        tiles.settlement[index] = aetheria::world::SettlementTier::Town;
        tiles.temperature[index] = static_cast<std::uint8_t>(17U);
        return tiles;
    };

    const auto run_zone_probe = [&](aetheria::zone::ZoneStore& zone_store, bool load_existing,
                                    aetheria::zone::FileZoneStore* file_store) {
        aetheria::zone::ZoneManager zone_manager{zone_store};
        for (const auto key : {region, site, local}) {
            if (load_existing) {
                static_cast<void>(zone_manager.require(key));
            } else {
                const auto handle = zone_manager.materialize(key);
                static_cast<void>(zone_manager.with(handle, [&](aetheria::zone::Zone& zone) {
                    if (aetheria::zone::level_of(key) == aetheria::zone::ZoneLevel::Region) {
                        auto& layers = std::get<aetheria::zone::RegionPayload>(zone.payload).layers;
                        layers.emplace(0, make_probe_region());
                    } else if (aetheria::zone::level_of(key) == aetheria::zone::ZoneLevel::Site) {
                        auto probe_region = make_probe_region();
                        auto materialized = aetheria::site::materialize_site_zone(
                            probe_region, {4, 7}, UINT64_C(0xA37E12A), 1, ruleset);
                        zone.payload = std::move(materialized.payload);
                        zone.lod = materialized.lod;
                    }
                }));
            }
        }
        zone_manager.tick(aetheria::time::Tick{requested_tick});
        zone_manager.save_all();
        if (file_store != nullptr) {
            auto manifest = file_store->manifest().value_or(aetheria::zone::SaveManifest{});
            manifest.world_seed = UINT64_C(0xA37E12A);
            manifest.raws_hash = aetheria::runtime::save_raws_hash(save_directory);
            manifest.now = aetheria::time::Tick{requested_tick};
            file_store->write_manifest(manifest);
        }

        std::cout << "zone tree:\n";
        for (const auto key : zone_manager.loaded_keys()) {
            const auto level = static_cast<unsigned>(aetheria::zone::level_value_of(key));
            const auto handle = zone_manager.get(key);
            std::uint64_t hash{};
            static_cast<void>(zone_manager.with(*handle, [&](const aetheria::zone::Zone& zone) {
                hash = aetheria::serialize::persistent_state_hash(zone, ruleset);
            }));
            std::cout << "  level=" << level << " key=" << aetheria::zone::value_of(key)
                      << " parent=" << aetheria::zone::value_of(aetheria::zone::parent_of(key))
                      << " hash=" << hash << '\n';
        }
    };

    if (save_directory.empty()) {
        aetheria::zone::InMemoryZoneStore zone_store{ruleset};
        run_zone_probe(zone_store, false, nullptr);
    } else {
        aetheria::zone::FileZoneStore zone_store{save_directory, ruleset};
        const bool load_existing = zone_store.manifest().has_value();
        run_zone_probe(zone_store, load_existing, &zone_store);
    }
    return 0;
}

} // namespace aetheria::sim::detail
