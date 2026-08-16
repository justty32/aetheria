#include "core/api/version.h"
#include "core/time/tick.h"

#include <array>
#include <cstdint>
#include <iostream>

#include <CLI/CLI.hpp>

int main(int argc, char** argv) {
    CLI::App app{"Aetheria headless M0 calendar probe"};
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
}

