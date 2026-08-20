#include "core/api/version.h"
#include "core/rules/ruleset.h"
#include "core/serialize/zone_codec.h"
#include "core/time/tick.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/zone_manager.h"
#include "sim/gen_commands.h"

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
    std::uint64_t generation_seed{};
    std::uint32_t generation_region_id{};
    std::uint16_t erosion_iterations{12};
    std::int16_t biome_moisture_bias{};
    std::string dump_stages;
    std::uint32_t verify_iterations{100};
    app.add_option("--tick", requested_tick, "額外換算的 Tick（秒）");
    app.add_option("--save-dir", save_directory, "跨程序 zone 存檔目錄");
    app.add_option("--data-dir", data_directory, "Ruleset TOML 資料目錄");
    auto* gen = app.add_subcommand("gen", "程序生成除錯工具");
    gen->require_subcommand(1);
    auto* gen_region = gen->add_subcommand("region", "生成一個 Region 並輸出階段雜湊");
    gen_region->add_option("--seed", generation_seed, "世界 seed")->required();
    gen_region->add_option("--region", generation_region_id, "Region id");
    gen_region->add_option("--erosion-iterations", erosion_iterations, "固定熱力侵蝕次數");
    gen_region->add_option("--biome-moisture-bias", biome_moisture_bias,
                           "biome 查表前的水氣偏移（階段 6 隔離探針）");
    gen_region->add_option("--dump-stages", dump_stages, "十二階段 PGM 輸出目錄");
    auto* gen_verify = gen->add_subcommand("verify", "重複生成並驗證同 seed 決定論");
    gen_verify->add_option("--seed", generation_seed, "起始世界 seed")->required();
    gen_verify->add_option("--iterations", verify_iterations, "驗證 seed 數量");
    CLI11_PARSE(app, argc, argv);

    const auto ruleset = aetheria::rules::RulesetLoader::load(data_directory);

    if (*gen_region) {
        return aetheria::sim::run_gen_region(ruleset, generation_seed, generation_region_id,
                                             erosion_iterations, biome_moisture_bias, dump_stages);
    }
    if (*gen_verify) {
        return aetheria::sim::run_gen_verify(ruleset, generation_seed, verify_iterations);
    }

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
                        auto& layers = std::get<aetheria::zone::RegionPayload>(zone.payload).layers;
                        auto& tiles =
                            layers.emplace(0, aetheria::world::RegionTiles{2, 2}).first->second;
                        tiles.temperature.at(0) = static_cast<std::uint8_t>(17U);
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
