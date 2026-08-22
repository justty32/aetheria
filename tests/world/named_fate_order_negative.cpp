#include "core/world/named_fate.h"

#include <cstdint>
#include <iostream>
#include <random>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

namespace {

struct CollidingHash {
    [[nodiscard]] std::size_t operator()(std::uint64_t) const noexcept { return 0; }
};

using OutcomeMap =
    std::unordered_map<std::uint64_t, aetheria::world::FateOutcome, CollidingHash>;

// 刻意錯誤：共享一條 RNG 並直接依 unordered_map 迭代順序把結果派給 uid。
[[nodiscard]] OutcomeMap broken_resolve(const std::vector<std::uint64_t>& insertion_order) {
    std::unordered_map<std::uint64_t, bool, CollidingHash> members;
    members.reserve(insertion_order.size());
    for (const auto uid : insertion_order) {
        members.emplace(uid, true);
    }
    std::mt19937_64 rng{UINT64_C(0xBAD0D3)};
    OutcomeMap result;
    for (const auto& [uid, marked] : members) {
        static_cast<void>(marked);
        result.emplace(uid, aetheria::world::roll_fate_outcome(
                                rng, 2'000, aetheria::world::FateRules{}));
    }
    return result;
}

TEST(NamedFateOrderNegative, UnorderedIterationChangesWhichPersonReceivesWhichRoll) {
    std::vector<std::uint64_t> ascending;
    std::vector<std::uint64_t> descending;
    for (std::uint64_t uid = 1; uid <= 50; ++uid) {
        ascending.push_back(uid);
        descending.push_back(51 - uid);
    }
    const auto first = broken_resolve(ascending);
    const auto second = broken_resolve(descending);
    std::uint32_t mismatched{};
    for (const auto& [uid, outcome] : first) {
        mismatched += second.at(uid) != outcome ? 1U : 0U;
    }
    std::cout << "named_fate_negative mismatched_entities=" << mismatched
              << " container=unordered_map\n";
    EXPECT_EQ(mismatched, 0U) << "unordered_map 迭代順序改變了具名角色拿到的擲骰";
}

}  // namespace
