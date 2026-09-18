#include "tests/site/site_unload_equivalence_test_support.h"

namespace {

using namespace aetheria::tests::site_unload_equivalence;

struct ControlledUnloadResult {
    TileQuantities quantities;
    std::uint32_t unloads{};
    std::uint32_t live_xun{};
    std::uint32_t absent_xun{};
};

[[nodiscard]] ControlledUnloadResult run_controlled_unloads(std::uint32_t unload_count) {
    constexpr std::uint32_t total_xun = 20;
    constexpr std::uint32_t controlled_live_xun = 10;
    constexpr std::uint32_t controlled_absent_xun = 10;
    if (unload_count == 0) {
        return {run_full(total_xun).quantities, 0, total_xun, 0};
    }
    if (controlled_absent_xun % std::min(unload_count, controlled_absent_xun) != 0) {
        throw std::invalid_argument{"診斷卸載次數不能整分固定的離線旬數"};
    }

    LifecycleFixture fixture;
    initialize_lifecycle_fixture(fixture);
    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(fixture.site);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        test_ruleset());
    if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
            aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
        })) {
        throw std::runtime_error{"控制卸載診斷無法建立 L_FULL Site"};
    }
    aetheria::site::SiteTurnPipeline site_pipeline{test_ruleset(), store};
    aetheria::world::RegionTurnPipeline region_pipeline{test_ruleset(), store};
    ControlledUnloadResult result;
    const auto productive_unloads = std::min(unload_count, controlled_absent_xun);
    const auto live_chunk = controlled_live_xun / productive_unloads;
    const auto absent_chunk = controlled_absent_xun / productive_unloads;
    const auto zero_time_unloads = unload_count - productive_unloads;

    const auto unload_and_reload = [&](std::uint32_t absent_xun) {
        aetheria::site::unload_site_zone(manager, handle, tiles, kReductionCoordinate,
                                         kReductionWorldSeed, kReductionRegionId,
                                         region_now(fixture.region), test_ruleset());
        ++result.unloads;
        for (std::uint32_t xun = 0; xun < absent_xun; ++xun) {
            region_pipeline.advance_xun(fixture.region);
            ++result.absent_xun;
        }
        handle = aetheria::site::rematerialize_site_zone(
            manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
            region_now(fixture.region), test_ruleset());
        if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
                aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
            })) {
            throw std::runtime_error{"控制卸載診斷重載失敗"};
        }
    };

    for (std::uint32_t episode = 0; episode < productive_unloads; ++episode) {
        if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
                static_cast<void>(site_pipeline.advance_hours(
                    site, fixture.region, 0, kReductionCoordinate, live_chunk * 240U));
            })) {
            throw std::runtime_error{"控制卸載診斷找不到 live Site"};
        }
        result.live_xun += live_chunk;
        unload_and_reload(absent_chunk);
        if (episode < zero_time_unloads) {
            unload_and_reload(0);
        }
    }
    if (!manager.with(handle, [&](const aetheria::zone::Zone& site) {
            aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site, test_ruleset());
        })) {
        throw std::runtime_error{"控制卸載診斷終點歸約失敗"};
    }
    if (result.unloads != unload_count || result.live_xun + result.absent_xun != total_xun) {
        throw std::runtime_error{"控制卸載診斷沒有守住卸載次數或總旬數"};
    }
    result.quantities = quantities(tiles);
    return result;
}

TEST(SiteLifecycle, ReportsBiasByUnloadCountAtFixedTwentyXun) {
    constexpr std::array<std::uint32_t, 5> unload_counts{0, 2, 5, 10, 20};
    const auto baseline = run_controlled_unloads(0);
    for (const auto unloads : unload_counts) {
        const auto observed = run_controlled_unloads(unloads);
        const auto bias = [](std::uint64_t value, std::uint64_t reference) {
            return (static_cast<double>(value) - static_cast<double>(reference)) /
                   std::max(1.0, static_cast<double>(reference));
        };
        EXPECT_EQ(observed.unloads, unloads);
        EXPECT_EQ(observed.live_xun + observed.absent_xun, 20U);
        std::cout << "site_unload_count_bias unloads=" << unloads
                  << " total_xun=20 live_xun=" << observed.live_xun
                  << " absent_xun=" << observed.absent_xun
                  << " population_bias="
                  << bias(observed.quantities.population, baseline.quantities.population)
                  << " development_bias="
                  << bias(observed.quantities.development, baseline.quantities.development)
                  << " food_bias=" << bias(observed.quantities.food, baseline.quantities.food)
                  << " production_bias="
                  << bias(observed.quantities.production, baseline.quantities.production)
                  << '\n';
    }
}

}  // namespace
