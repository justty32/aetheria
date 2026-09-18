#include "tests/site/site_unload_equivalence_test_support.h"

namespace {

using namespace aetheria::tests::site_unload_equivalence;

struct SequenceResult {
    TileQuantities quantities;
    std::uint32_t live_xun{};
    std::uint32_t absent_xun{};
    std::uint32_t transitions{};
};

[[nodiscard]] SequenceResult run_random_sequence(std::uint64_t random_bits,
                                                 std::uint32_t xun_count) {
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
        throw std::runtime_error{"隨機序列無法建立初始 L_FULL Site"};
    }
    aetheria::site::SiteTurnPipeline site_pipeline{test_ruleset(), store};
    aetheria::world::RegionTurnPipeline region_pipeline{test_ruleset(), store};
    bool live = true;
    SequenceResult result;
    for (std::uint32_t xun = 0; xun < xun_count; ++xun) {
        const bool next_live = ((random_bits >> xun) & UINT64_C(1)) != 0;
        if (next_live != live) {
            ++result.transitions;
        }
        if (next_live) {
            if (!live) {
                handle = aetheria::site::rematerialize_site_zone(
                    manager, tiles, kReductionCoordinate, kReductionWorldSeed,
                    kReductionRegionId, region_now(fixture.region), test_ruleset());
                if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
                        aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
                    })) {
                    throw std::runtime_error{"隨機序列重載後無法進入 L_FULL"};
                }
                live = true;
            }
            if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
                    static_cast<void>(site_pipeline.advance_hours(
                        site, fixture.region, 0, kReductionCoordinate, 240));
                })) {
                throw std::runtime_error{"隨機序列找不到 live Site"};
            }
            ++result.live_xun;
        } else {
            if (live) {
                aetheria::site::unload_site_zone(manager, handle, tiles, kReductionCoordinate,
                                                 kReductionWorldSeed, kReductionRegionId,
                                                 region_now(fixture.region), test_ruleset());
                live = false;
            }
            region_pipeline.advance_xun(fixture.region);
            ++result.absent_xun;
        }
    }
    if (!live) {
        handle = aetheria::site::rematerialize_site_zone(
            manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
            region_now(fixture.region), test_ruleset());
        if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
                aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
            })) {
            throw std::runtime_error{"隨機序列終點重載失敗"};
        }
    }
    if (!manager.with(handle, [&](const aetheria::zone::Zone& site) {
            aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site, test_ruleset());
        })) {
        throw std::runtime_error{"隨機序列終點歸約失敗"};
    }
    result.quantities = quantities(tiles);
    return result;
}

struct Distribution {
    double mean{};
    double standard_deviation{};
};

[[nodiscard]] bool systematically_high(double mean, double baseline) noexcept {
    return mean > baseline;
}

template <typename Value>
[[nodiscard]] Distribution distribution(const std::vector<TileQuantities>& samples,
                                        Value TileQuantities::*member) {
    double sum{};
    for (const auto& sample : samples) {
        sum += static_cast<double>(sample.*member);
    }
    const auto mean = sum / static_cast<double>(samples.size());
    double squared_difference{};
    for (const auto& sample : samples) {
        const auto difference = static_cast<double>(sample.*member) - mean;
        squared_difference += difference * difference;
    }
    return {mean, std::sqrt(squared_difference / static_cast<double>(samples.size()))};
}

void report_unbias_row(std::string_view name, double baseline,
                       const Distribution& observed) {
    const auto bias = (observed.mean - baseline) / std::max(1.0, baseline);
    const auto direction = bias > 0.0 ? "high" : bias < 0.0 ? "low" : "neutral";
    std::cout << "site_unbias_" << name << " baseline=" << baseline
              << " mean=" << observed.mean << " stddev=" << observed.standard_deviation
              << " bias=" << bias << " direction=" << direction << '\n';
    EXPECT_LT(std::abs(bias), 0.05) << name;
    EXPECT_FALSE(systematically_high(observed.mean, baseline))
        << name << " 反覆進出造成系統性偏高";
}

TEST(SiteLifecycle, HundredRandomLoadUnloadSequencesStayBoundedAndCannotBiasHigh) {
    constexpr std::size_t sample_count = 100;
    constexpr std::uint32_t xun_count = 20;
    const auto baseline = run_full(xun_count).quantities;
    std::vector<TileQuantities> samples;
    samples.reserve(sample_count);
    std::uint32_t live_xun{};
    std::uint32_t absent_xun{};
    std::uint32_t transitions{};
    for (std::size_t sample = 0; sample < sample_count; ++sample) {
        auto bits = aetheria::worldgen::splitmix64(UINT64_C(0x4D410001) ^ sample);
        bits |= UINT64_C(1);
        bits &= ~UINT64_C(2);
        const auto result = run_random_sequence(bits, xun_count);
        samples.push_back(result.quantities);
        live_xun += result.live_xun;
        absent_xun += result.absent_xun;
        transitions += result.transitions;
    }
    ASSERT_EQ(samples.size(), sample_count);
    EXPECT_GT(live_xun, 0U);
    EXPECT_GT(absent_xun, 0U);
    EXPECT_GE(transitions, sample_count);

    const auto population = distribution(samples, &TileQuantities::population);
    const auto development = distribution(samples, &TileQuantities::development);
    const auto food = distribution(samples, &TileQuantities::food);
    const auto production = distribution(samples, &TileQuantities::production);
    report_unbias_row("population", baseline.population, population);
    report_unbias_row("development", baseline.development, development);
    report_unbias_row("food", static_cast<double>(baseline.food), food);
    report_unbias_row("production", static_cast<double>(baseline.production), production);

    EXPECT_TRUE(systematically_high(static_cast<double>(baseline.population) * 1.01,
                                    baseline.population));
    std::cout << "site_unbias_sequences count=" << sample_count << " xun=" << xun_count
              << " live_xun=" << live_xun << " absent_xun=" << absent_xun
              << " transitions=" << transitions
              << " positive_control_direction=high positive_control_bias=0.01\n";
}

}  // namespace
