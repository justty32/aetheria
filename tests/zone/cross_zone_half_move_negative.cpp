#include "core/world/region_movement.h"
#include "core/zone/zone.h"

#include <cstddef>
#include <iostream>
#include <map>

#include <gtest/gtest.h>

namespace {

// 這是刻意錯誤的負向控制：先刪來源，才發現目的 uid 衝突並回滾目的。
[[nodiscard]] bool broken_half_move(aetheria::zone::Zone& source, entt::entity entity,
                                    aetheria::zone::Zone& destination) {
    const auto stable = source.reg.get<const aetheria::world::StableId>(entity);
    const auto points = source.reg.get<const aetheria::world::MovementPoints>(entity);
    const auto staged = destination.reg.create();
    destination.reg.emplace<aetheria::world::StableId>(staged, stable);
    destination.reg.emplace<aetheria::world::MovementPoints>(staged, points);
    source.reg.destroy(entity);  // bug：commit 順序錯了。
    if (destination.uid_index.contains(stable.uid)) {
        destination.reg.destroy(staged);
        return false;
    }
    return true;
}

TEST(CrossZoneHalfMoveNegative, DetectsLostSourceAndMissingDestination) {
    constexpr auto region =
        aetheria::zone::child_key(aetheria::zone::kRootZone, 8, 0);
    constexpr auto site = aetheria::zone::child_key(region, 1, 1);
    aetheria::zone::Zone source{aetheria::zone::child_key(site, 1, 1)};
    aetheria::zone::Zone destination{aetheria::zone::child_key(site, 2, 1)};
    const auto actor = source.reg.create();
    source.reg.emplace<aetheria::world::StableId>(actor, 404);
    source.reg.emplace<aetheria::world::MovementPoints>(actor, 5, 9);
    source.uid_index.emplace(404, actor);
    const auto incumbent = destination.reg.create();
    destination.reg.emplace<aetheria::world::StableId>(incumbent, 404);
    destination.uid_index.emplace(404, incumbent);

    EXPECT_FALSE(broken_half_move(source, actor, destination));
    const bool source_valid = source.reg.valid(actor);
    std::size_t destination_migrated{};
    for ([[maybe_unused]] const auto entity :
         destination.reg.view<const aetheria::world::StableId,
                              const aetheria::world::MovementPoints>()) {
        ++destination_migrated;
    }
    std::cout << "half_move_negative source_valid=" << source_valid
              << " destination_migrated=" << destination_migrated << '\n';

    EXPECT_TRUE(source_valid) << "來源實體不可在失敗交易中消失";
    EXPECT_EQ(destination_migrated, 1U) << "目的必須含搬入的非空 component 實體";
}

}  // namespace
