#include "core/api/version.h"
#include "core/rules/ruleset.h"
#include "core/serialize/zone_codec.h"
#include "core/time/tick.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/zone_manager.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

#include <CLI/CLI.hpp>

int main(int argc, char** argv) {
    CLI::App app{"Aetheria headless core probe"};
    std::int64_t requested_tick = 31'104'000;
    std::string save_directory;
    std::string data_directory{AETHERIA_DEFAULT_DATA_DIR};
    app.add_option("--tick", requested_tick, "額外換算的 Tick（秒）");
    app.add_option("--save-dir", save_directory, "跨程序 zone 存檔目錄");
    app.add_option("--data-dir", data_directory, "Ruleset TOML 資料目錄");
    CLI11_PARSE(app, argc, argv);

    const auto ruleset = aetheria::rules::RulesetLoader::load(data_directory);

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
                        zone.region_tiles.emplace(2, 2);
                        zone.region_tiles->temperature.at(0) = static_cast<std::uint8_t>(17U);
                    }
                }));
            }
        }
        zone_manager.tick(aetheria::time::Tick{requested_tick});
        zone_manager.save_all();
        if (file_store != nullptr) {
            auto manifest = file_store->manifest().value_or(aetheria::zone::SaveManifest{});
            manifest.world_seed = UINT64_C(0xA37E12A);
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
}
