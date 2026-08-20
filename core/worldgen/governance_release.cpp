#include "core/worldgen/influence_spread.h"

#include <stdexcept>

namespace aetheria::worldgen {

std::vector<world::FactionId>
release_beyond_governance(const InfluenceClaimMap& claims, std::int64_t governance_max_cost) {
    if (claims.owner.size() != claims.capital_cost.size() || governance_max_cost < 0) {
        throw std::invalid_argument{"治理距離釋回需要等長認領資料與非負門檻"};
    }
    auto owners = claims.owner;
    for (std::size_t index = 0; index < owners.size(); ++index) {
        if (claims.capital_cost[index] > governance_max_cost) {
            owners[index] = world::FactionId{0};
        }
    }
    return owners;
}

}  // namespace aetheria::worldgen
