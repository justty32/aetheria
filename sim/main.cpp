#include "core/api/version.h"
#include "core/time/tick.h"
#include "core/zone/zone_manager.h"

#include <array>
#include <cstdint>
#include <iostream>

#include <CLI/CLI.hpp>

int main(int argc, char** argv) {
    CLI::App app{"Aetheria headless core probe"};
    std::int64_t requested_tick = 31'104'000;
    app.add_option("--tick", requested_tick, "額外換算的 Tick（秒）");
    CLI11_PARSE(app, argc, argv);

    std::cout << "Aetheria core " << aetheria::core_version() << '\n';
    const std::array ticks{std::int64_t{0}, std::int64_t{864'000}, requested_tick};
    for (const auto raw_tick : ticks) {
        const auto date = aetheria::time::to_date(aetheria::time::Tick{raw_tick});
        std::cout << "tick=" << raw_tick << " -> year=" << date.year
                  << " season=" << static_cast<unsigned>(date.season)
                  << " month=" << static_cast<unsigned>(date.month)
                  << " xun=" << static_cast<unsigned>(date.xun) << '\n';
    }

    aetheria::zone::InMemoryZoneStore zone_store;
    aetheria::zone::ZoneManager zone_manager{zone_store};
    const auto region = aetheria::zone::child_key(aetheria::zone::kRootZone, 1, 0);
    const auto site = aetheria::zone::child_key(region, 4, 7);
    const auto local = aetheria::zone::child_key(site, 12, 9);
    static_cast<void>(zone_manager.materialize(region));
    static_cast<void>(zone_manager.materialize(site));
    static_cast<void>(zone_manager.materialize(local));

    std::cout << "zone tree:\n";
    for (const auto key : zone_manager.loaded_keys()) {
        const auto level = static_cast<unsigned>(aetheria::zone::level_value_of(key));
        std::cout << "  level=" << level << " key=" << aetheria::zone::value_of(key)
                  << " parent=" << aetheria::zone::value_of(aetheria::zone::parent_of(key)) << '\n';
    }
}
