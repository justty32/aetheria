#include "core/local/local_reduction.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <type_traits>

namespace {

using aetheria::local::ControlPointState;
using aetheria::local::GatheringPointState;
using aetheria::local::LocalReductionState;
using aetheria::local::PassageState;
using aetheria::local::StructureSegmentState;
using aetheria::site::ControllerReduction;
using aetheria::site::PassabilityCostReduction;
using aetheria::site::ResourceYieldModifierReduction;
using aetheria::site::StructureIntegrityReduction;

static_assert(std::tuple_size_v<aetheria::site::LocalReductionRows> == 4);
static_assert(!std::is_default_constructible_v<aetheria::site::LocalTileDelta>);

[[nodiscard]] aetheria::zone::ZoneKey sample_site_key() {
    const auto region = aetheria::zone::child_key(aetheria::zone::kRootZone, 7, 0);
    return aetheria::zone::child_key(region, 4, 9);
}

[[nodiscard]] aetheria::zone::Zone sample_local(aetheria::site::SiteXY coordinate,
                                                LocalReductionState reduction) {
    aetheria::zone::Zone result{
        aetheria::zone::child_key(sample_site_key(), coordinate.x, coordinate.y)};
    std::get<aetheria::zone::LocalPayload>(result.payload).reduction = std::move(reduction);
    return result;
}

[[nodiscard]] LocalReductionState non_empty_state() {
    return {
        .structure_segments = {{false}, {false}, {false}, {true}},
        .control_points = {{aetheria::world::FactionId{2}},
                           {aetheria::world::FactionId{3}},
                           {aetheria::world::FactionId{2}}},
        .gathering_points = {{50, 100}, {100, 100}},
        .passages = {{200}, {150}},
    };
}

TEST(LocalReduction, NonEmptyLocalMeasuresExactlyFourRowsAndAppliesToSiteTile) {
    constexpr aetheria::site::SiteXY coordinate{32, 32};
    aetheria::site::SiteLayers parent;
    const auto state = non_empty_state();
    const auto local = sample_local(coordinate, state);
    const auto writes = aetheria::local::reduce_live_local(parent, coordinate, local);
    const auto index = static_cast<std::size_t>(coordinate.y) * aetheria::site::kSiteWidth +
                       coordinate.x;

    EXPECT_EQ(writes, 4U);
    EXPECT_EQ(parent.local_reductions.value<StructureIntegrityReduction>(index), 75U);
    EXPECT_EQ(parent.local_reductions.value<ControllerReduction>(index),
              aetheria::world::FactionId{2});
    EXPECT_EQ(parent.local_reductions.value<ResourceYieldModifierReduction>(index), 75U);
    EXPECT_EQ(parent.local_reductions.value<PassabilityCostReduction>(index), 150U);

    std::cout << "local_reduction_non_empty structure_segments="
              << state.structure_segments.size() << " damaged_segments=1 control_points="
              << state.control_points.size() << " gathering_points="
              << state.gathering_points.size() << " passages=" << state.passages.size()
              << " integrity=75 controller=2 resource_modifier=75 passability_cost=150 writes="
              << writes << '\n';
}

TEST(LocalReduction, EmptyLocalMeansNoChangeInsteadOfZeroOrDefault) {
    constexpr aetheria::site::SiteXY coordinate{32, 32};
    aetheria::site::SiteLayers parent;
    const auto populated = sample_local(coordinate, non_empty_state());
    ASSERT_EQ(aetheria::local::reduce_live_local(parent, coordinate, populated), 4U);

    const auto empty = sample_local(coordinate, {});
    const auto delta = aetheria::local::ReductionTable::reduce(empty);
    EXPECT_FALSE(delta.value<StructureIntegrityReduction>().has_value());
    EXPECT_FALSE(delta.value<ControllerReduction>().has_value());
    EXPECT_FALSE(delta.value<ResourceYieldModifierReduction>().has_value());
    EXPECT_FALSE(delta.value<PassabilityCostReduction>().has_value());
    const auto writes = aetheria::local::reduce_live_local(parent, coordinate, empty);
    const auto index = static_cast<std::size_t>(coordinate.y) * aetheria::site::kSiteWidth +
                       coordinate.x;

    EXPECT_EQ(writes, 0U);
    EXPECT_EQ(parent.local_reductions.value<StructureIntegrityReduction>(index), 75U);
    EXPECT_EQ(parent.local_reductions.value<ControllerReduction>(index),
              aetheria::world::FactionId{2});
    EXPECT_EQ(parent.local_reductions.value<ResourceYieldModifierReduction>(index), 75U);
    EXPECT_EQ(parent.local_reductions.value<PassabilityCostReduction>(index), 150U);
    std::cout << "local_reduction_empty observations=0 writes=0 retained=75,2,75,150\n";
}

TEST(LocalReduction, AbsoluteSnapshotCannotCountTheSamePassabilityChangeTwice) {
    constexpr aetheria::site::SiteXY coordinate{7, 11};
    const auto index = static_cast<std::size_t>(coordinate.y) * aetheria::site::kSiteWidth +
                       coordinate.x;
    aetheria::site::SiteLayers parent;
    LocalReductionState state;
    state.passages = {{200}};
    ASSERT_EQ(aetheria::local::ReductionTable::apply(
                  parent, coordinate, aetheria::local::ReductionTable::reduce(state)),
              1U);
    state.passages = {{175}};
    ASSERT_EQ(aetheria::local::ReductionTable::apply(
                  parent, coordinate, aetheria::local::ReductionTable::reduce(state)),
              1U);
    state.passages = {{150}};
    const auto final_delta = aetheria::local::ReductionTable::reduce(state);
    ASSERT_EQ(aetheria::local::ReductionTable::apply(parent, coordinate, final_delta), 1U);
    ASSERT_EQ(aetheria::local::ReductionTable::apply(parent, coordinate, final_delta), 1U);

    const auto actual = parent.local_reductions.value<PassabilityCostReduction>(index);
    constexpr std::uint16_t double_count_counterfactual = 125;
    EXPECT_EQ(actual, 150U);
    EXPECT_NE(actual, double_count_counterfactual);
    std::cout << "local_reduction_no_double_count baseline=200 first=175 final=150 "
                 "reapplied_final=150 double_count_counterfactual=125\n";
}

TEST(LocalReduction, SemanticInputOrderHasTheSameHashAndChangedInputDoesNot) {
    auto forward = non_empty_state();
    auto reverse = forward;
    std::ranges::reverse(reverse.structure_segments);
    std::ranges::reverse(reverse.control_points);
    std::ranges::reverse(reverse.gathering_points);
    std::ranges::reverse(reverse.passages);
    auto changed = forward;
    changed.passages.back().traversal_cost = 149;

    const auto forward_hash = aetheria::local::ReductionTable::reduce(forward).hash();
    const auto repeat_hash = aetheria::local::ReductionTable::reduce(forward).hash();
    const auto reverse_hash = aetheria::local::ReductionTable::reduce(reverse).hash();
    const auto changed_hash = aetheria::local::ReductionTable::reduce(changed).hash();
    EXPECT_EQ(forward_hash, repeat_hash);
    EXPECT_EQ(forward_hash, reverse_hash);
    EXPECT_NE(forward_hash, changed_hash);
    std::cout << "local_reduction_hash forward=" << forward_hash
              << " repeat=" << repeat_hash << " reordered=" << reverse_hash
              << " changed=" << changed_hash << '\n';
}

TEST(LocalReduction, InvalidGatheringPointIsRejected) {
    LocalReductionState invalid;
    invalid.gathering_points = {{101, 100}};
    EXPECT_THROW(static_cast<void>(aetheria::local::ReductionTable::reduce(invalid)),
                 std::invalid_argument);
}

}  // namespace
