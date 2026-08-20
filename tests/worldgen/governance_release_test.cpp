#include "core/worldgen/influence_spread.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::world::FactionId;
using aetheria::worldgen::InfluenceClaimMap;

[[nodiscard]] std::vector<FactionId>
release_in_order(const InfluenceClaimMap& claims, std::int64_t maximum_cost,
                 std::vector<std::size_t> order) {
    auto owners = claims.owner;
    for (const auto index : order) {
        if (claims.capital_cost.at(index) > maximum_cost) {
            owners.at(index) = FactionId{0};
        }
    }
    return owners;
}

[[nodiscard]] std::vector<FactionId>
order_dependent_fringe_release(const InfluenceClaimMap& claims, std::int64_t maximum_cost,
                               std::vector<std::size_t> order) {
    auto owners = claims.owner;
    for (const auto index : order) {
        const bool touches_unowned = (index > 0 && owners[index - 1U] == FactionId{0}) ||
                                     (index + 1U < owners.size() &&
                                      owners[index + 1U] == FactionId{0});
        if (claims.capital_cost.at(index) > maximum_cost && touches_unowned) {
            owners.at(index) = FactionId{0};
        }
    }
    return owners;
}

TEST(GovernanceRelease, IsPointwiseAndOrderIndependentWithMutableFringeNegativeControl) {
    const auto infinity = std::numeric_limits<std::int64_t>::max();
    const InfluenceClaimMap claims{
        {FactionId{0}, FactionId{1}, FactionId{1}, FactionId{1}, FactionId{1}, FactionId{0}},
        {infinity, 20, 30, 40, 50, infinity},
    };
    std::vector<std::size_t> forward{0, 1, 2, 3, 4, 5};
    auto reverse = forward;
    std::ranges::reverse(reverse);

    const auto canonical = aetheria::worldgen::release_beyond_governance(claims, 25);
    EXPECT_EQ(canonical, release_in_order(claims, 25, forward));
    EXPECT_EQ(canonical, release_in_order(claims, 25, reverse));
    EXPECT_NE(order_dependent_fringe_release(claims, 25, forward),
              order_dependent_fringe_release(claims, 25, reverse));
    EXPECT_EQ(canonical,
              (std::vector{FactionId{0}, FactionId{1}, FactionId{0}, FactionId{0},
                           FactionId{0}, FactionId{0}}));
}

TEST(GovernanceRelease, RejectsMismatchedClaimsAndNegativeThreshold) {
    const InfluenceClaimMap mismatched{{FactionId{1}}, {}};
    const InfluenceClaimMap valid{{FactionId{1}}, {0}};
    EXPECT_THROW(
        static_cast<void>(aetheria::worldgen::release_beyond_governance(mismatched, 10)),
        std::invalid_argument);
    EXPECT_THROW(static_cast<void>(aetheria::worldgen::release_beyond_governance(valid, -1)),
                 std::invalid_argument);
}

}  // namespace
