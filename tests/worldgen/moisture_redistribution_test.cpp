#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::MoistureRedistributionParams;
using aetheria::worldgen::QuantizedElevation;
using aetheria::worldgen::redistribute;
using aetheria::worldgen::RegionSlowVariables;
using aetheria::worldgen::RiverStageOutput;

// 一列五格：0 號是海，1..4 是陸地；2..4 削頂在 65535，只靠 flow 才分得開。
[[nodiscard]] RiverStageOutput clipped_row() {
    return RiverStageOutput{5,
                            1,
                            {4000, 4200, 4200, 4200, 4200},
                            {-1, -1, -1, -1, -1},
                            {1, 1, 30, 10, 20},
                            {0, 0, 0, 0, 0},
                            {UINT16_MAX, 10000, UINT16_MAX, UINT16_MAX, UINT16_MAX},
                            {0, 0, 0, 0, 0}};
}

[[nodiscard]] QuantizedElevation land_mask(std::vector<std::uint8_t> land) {
    const auto width = static_cast<std::uint32_t>(land.size());
    return QuantizedElevation{width, 1, std::vector<std::uint16_t>(land.size(), 4200),
                              std::move(land), 4096};
}

TEST(MoistureRedistribution, InjectedTransformStillReachesTheSeam) {
    const auto raw = clipped_row();
    const auto shifted = redistribute(
        raw, MoistureRedistributionParams{},
        [](RiverStageOutput& field, const MoistureRedistributionParams&) {
            ++field.moisture.front();
        });

    EXPECT_NE(hash_stage(shifted), hash_stage(raw));
}

TEST(MoistureRedistribution, SplitsTheClippedGroupByFlowAndLeavesOceanAlone) {
    const auto result =
        redistribute(clipped_row(), land_mask({0, 1, 1, 1, 1}), MoistureRedistributionParams{});

    // 陸地 rank 0..3 攤平到 [10000, 65535]：10000、28512、47023、65535。
    EXPECT_EQ(result.moisture,
              (std::vector<std::uint16_t>{UINT16_MAX, 10000, 65535, 28512, 47023}));
}

TEST(MoistureRedistribution, FullKeyTiesShareOneValueRegardlessOfCellIndex) {
    auto raw = clipped_row();
    raw.flow = {1, 1, 10, 10, 10};
    const auto result =
        redistribute(raw, land_mask({0, 1, 1, 1, 1}), MoistureRedistributionParams{});

    // 三格同鍵，取 rank 1..3 的中點 2 → 10000 + 2/3 × 55535。
    EXPECT_EQ(result.moisture,
              (std::vector<std::uint16_t>{UINT16_MAX, 10000, 47023, 47023, 47023}));
}

TEST(MoistureRedistribution, ReferenceSeedNoLongerSaturatesHalfTheLand) {
    const auto result = build_skeleton(RegionSlowVariables{0, 128, 96, 35}, UINT64_C(515151),
                                       aetheria::tests::test_ruleset());
    const auto& land = result.skeleton.elevation.land;
    std::size_t land_count{};
    std::size_t saturated{};
    for (std::size_t index = 0; index < land.size(); ++index) {
        if (land[index] != 0) {
            ++land_count;
            saturated += result.rivers.moisture[index] == UINT16_MAX;
        }
    }
    ASSERT_GT(land_count, 0U);
    // 等化前 41.8%（1541/3686）。rank remap 後只剩最濕的一個同鍵群能落在頂端；
    // 允許 1% 是同鍵群的上界，不是調出來的數。
    EXPECT_LT(saturated * 100U, land_count);
}

}  // namespace
