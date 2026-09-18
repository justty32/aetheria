#pragma once

#include <aetheria/runtime/cross_zone.h>

#include "core/local/local_materialize.h"
#include "core/serialize/normalized_state_hash.h"
#include "core/world/region_movement.h"
#include "core/zone/zone.h"
#include "core/zone/zone_manager.h"
#include "tests/local/local_test_support.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

#include <gtest/gtest.h>

namespace aetheria::tests::cross_zone {

using aetheria::local::LocalXY;
using aetheria::runtime::CrossZoneRuntime;
using aetheria::runtime::EntityRef;
using aetheria::runtime::LocalPosition;
using aetheria::tests::open_site_layer;
using aetheria::tests::test_ruleset;
using aetheria::world::MovementPoints;
using aetheria::world::StableId;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::Zone;
using aetheria::zone::ZoneKey;
using aetheria::zone::ZoneManager;

inline constexpr auto kRegion = aetheria::zone::child_key(aetheria::zone::kRootZone, 7, 0);

inline constexpr auto kSite = aetheria::zone::child_key(kRegion, 4, 5);

inline constexpr auto kSource = aetheria::zone::child_key(kSite, 11, 12);

inline constexpr auto kDestination = aetheria::zone::child_key(kSite, 12, 12);

[[nodiscard]] inline ZoneManager::ZoneMaterializer local_materializer(
    const aetheria::site::SiteProceduralLayer& parent) {
    return [&parent](ZoneKey key, std::unique_ptr<Zone> persistent) {
        if (aetheria::zone::parent_of(key) != kSite) {
            return std::unique_ptr<Zone>{};
        }
        const aetheria::site::SiteXY coordinate{
            static_cast<std::uint16_t>(aetheria::zone::local_x_of(key)),
            static_cast<std::uint16_t>(aetheria::zone::local_y_of(key))};
        const auto feature = *test_ruleset().find_feature("feature.none");
        auto generated = aetheria::local::materialize_local_zone(
            kSite, parent, coordinate, aetheria::tests::kLocalSiteSeed, feature,
            test_ruleset());
        if (persistent == nullptr) {
            return std::make_unique<Zone>(std::move(generated));
        }
        if (persistent->key != key) {
            return std::unique_ptr<Zone>{};
        }
        persistent->payload = std::move(generated.payload);
        persistent->lod = generated.lod;
        return persistent;
    };
}

[[nodiscard]] inline std::unique_ptr<Zone> local_zone(ZoneKey key, std::uint16_t marker = 0) {
    auto result = std::make_unique<Zone>(key);
    aetheria::local::LocalTiles tiles;
    tiles.ground.assign(aetheria::local::kLocalTileCount,
                        aetheria::rules::GroundId{marker});
    tiles.overlay.assign(aetheria::local::kLocalTileCount,
                         aetheria::local::OverlayId::Vegetation);
    tiles.occupant.assign(aetheria::local::kLocalTileCount, marker);
    tiles.edges.assign(aetheria::local::kLocalTileCount * 4U,
                       aetheria::rules::EdgeId{marker});
    tiles.light.assign(aetheria::local::kLocalTileCount,
                       static_cast<std::uint8_t>(marker));
    std::get<aetheria::zone::LocalPayload>(result->payload).layers.emplace(0,
                                                                          std::move(tiles));
    return result;
}

[[nodiscard]] inline entt::entity add_actor(Zone& zone, std::uint64_t uid,
                                     LocalXY position = {1, 2}) {
    const auto entity = zone.reg.create();
    zone.reg.emplace<StableId>(entity, uid);
    zone.reg.emplace<MovementPoints>(entity, 7, 13);
    zone.reg.emplace<LocalPosition>(entity, position);
    return entity;
}

[[nodiscard]] inline entt::entity add_persistent_object(Zone& zone, std::uint64_t uid) {
    const auto entity = zone.reg.create();
    zone.reg.emplace<StableId>(entity, uid);
    return entity;
}

}  // namespace aetheria::tests::cross_zone
