// playable_session_scenario.cpp：場景初始化。

#include "core/runtime/playable_session.h"

#include "core/local/dungeon.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "core/worldgen/region_generator.h"


#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>


#include "core/runtime/playable_session_detail.h"

namespace aetheria::runtime {
using namespace detail;

namespace {

[[nodiscard]] bool covers(site::SiteXY tile,
                          const site::ProceduralBuilding& building) noexcept {
    return tile.x >= building.origin.x && tile.y >= building.origin.y &&
           tile.x < static_cast<std::uint32_t>(building.origin.x) + building.width &&
           tile.y < static_cast<std::uint32_t>(building.origin.y) + building.height;
}

} // namespace

namespace detail {

void install_coverage_dungeon_entrance(site::SiteLayers& layers,
                                       site::SiteXY entrance,
                                       const rules::Ruleset& ruleset) {
    const auto entrance_id = ruleset.find_building("building.dungeon_entrance");
    const auto* definition =
        entrance_id.has_value() ? ruleset.building(*entrance_id) : nullptr;
    if (definition == nullptr) {
        throw std::runtime_error{"可玩覆蓋情境缺少地城入口 def"};
    }
    std::erase_if(layers.procedural.buildings,
                  [&](const auto& building) { return covers(entrance, building); });
    layers.procedural.buildings.push_back(
        {*entrance_id, entrance, definition->frontage, definition->depth,
         site::SiteBoundarySide::North,
         site::ProceduralBuildingDamage::Intact});
    // 路線 C 以結構辨識地下設施；其地表再交由路線 B 生成，因此分區須維持 Open。
    layers.procedural.zoning[site_index(entrance)] = site::SiteZoning::Open;
}

} // namespace detail

void PlayableSession::initialize_scenario() {
    const auto build = worldgen::build_skeleton(
        worldgen::RegionSlowVariables{region_id_, 128, 96}, seed_, ruleset_);
    auto generated = worldgen::populate(build.skeleton,
                                        worldgen::RegionFastVariables{});
    const auto grass = ruleset_.find_terrain("terrain.grassland");
    const auto plain = ruleset_.find_relief("relief.plain");
    const auto no_feature = ruleset_.find_feature("feature.none");
    const auto no_edge = ruleset_.find_edge("edge.none");
    if (!grass || !plain || !no_feature || !no_edge) {
        throw std::runtime_error{"可玩情境缺少草原／平地／空地物／空邊規則"};
    }
    for (std::int16_t x = coverage_tile_.x; x <= enemy_start_.x; ++x) {
        const world::RegionXY coordinate{x, player_start_.y};
        const auto index = generated.index_of(coordinate);
        generated.base[index] = *grass;
        generated.relief[index] = *plain;
        generated.feature[index] = *no_feature;
        generated.owner[index] = x < battle_tile_.x ? world::FactionId{1}
                                                     : world::FactionId{2};
        for (std::size_t direction = 0; direction < 4; ++direction) {
            generated.edges[index * 4U + direction] = *no_edge;
        }
    }
    for (std::int16_t x = player_start_.x; x < enemy_start_.x; ++x) {
        generated.set_edge({x, player_start_.y},
                           {static_cast<std::int16_t>(x + 1), player_start_.y},
                           *no_edge);
    }
    generated.settlement[generated.index_of(battle_tile_)] =
        world::SettlementTier::Town;
    generated.settlement[generated.index_of(coverage_tile_)] =
        world::SettlementTier::Town;

    region_key_ = zone::child_key(zone::kRootZone, region_id_, 0);
    auto region = std::make_unique<zone::Zone>(region_key_);
    std::get<zone::RegionPayload>(region->payload).layers.emplace(
        0, std::move(generated));
    const auto placeholder = *region->reg.view<zone::ZoneMeta>().begin();
    region->reg.emplace<world::TurnClock>(placeholder, time::Tick{0});
    region->pinned = true;
    region_ = region.get();
    static_cast<void>(manager_->adopt(std::move(region)));

    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    auto battle_site = std::make_unique<zone::Zone>(site::materialize_site_zone(
        region_tiles, battle_tile_, seed_, region_id_, ruleset_));
    battle_site_key_ = battle_site->key;
    site::reduce_live_site_xun(region_tiles, battle_tile_, *battle_site, ruleset_);
    battle_site->pinned = true;
    battle_site_ = battle_site.get();
    static_cast<void>(manager_->adopt(std::move(battle_site)));

    const auto create_army = [&](world::StableId id, world::ArmyState state,
                                 world::RegionXY position, world::RegionXY target) {
        const auto entity = region_->reg.create();
        region_->reg.emplace<world::StableId>(entity, id);
        region_->reg.emplace<world::RegionPosition>(entity, 0, position);
        region_->reg.emplace<world::MovementPoints>(entity, 0, 4);
        region_->reg.emplace<world::ArmyState>(entity, state);
        region_->uid_index.emplace(id.uid, entity);
        armies_.push_back({id, entity});
        if (!state.player_controlled) {
            turn_pipeline_.issue_move(*region_, id, target);
        }
    };
    create_army(player_army_id_, {world::FactionId{1}, 120'000, true},
                player_start_, enemy_start_);
    create_army(enemy_army_id_, {world::FactionId{2}, 48'000, false},
                enemy_start_, player_start_);
    append_event(PlayableEventKind::NewGame, player_start_, region_id_, seed_);
    initialize_coverage_site();
}

void PlayableSession::initialize_coverage_site() {
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    auto coverage = site::materialize_site_zone(
        region_tiles, coverage_tile_, seed_, region_id_, ruleset_);
    coverage_site_key_ = coverage.key;
    coverage_local_key_ = zone::child_key(coverage_site_key_, local_entrance_.x,
                                          local_entrance_.y);

    auto& layers = std::get<zone::SitePayload>(coverage.payload).layers;
    layers.persistent.place_name_key = "place.mist_bridge";
    layers.persistent.dungeons.push_back(
        {UINT64_C(0xD006E002), "place.mist_bridge_dungeon", false, 6});

    install_coverage_dungeon_entrance(layers, local_entrance_, ruleset_);

    site::enter_full_site(coverage, region_tiles, coverage_tile_);
    site::reduce_live_site_xun(region_tiles, coverage_tile_, coverage, ruleset_);
    const auto initial_dungeon = local::generate_dungeon(
        seed_ ^ UINT64_C(0xD006E002), layers.persistent.dungeons.front(), {},
        ruleset_);
    dungeon_density_before_ = static_cast<std::uint16_t>(std::accumulate(
        initial_dungeon.floors.begin(), initial_dungeon.floors.end(), 0U,
        [](std::uint32_t total, const auto& floor) {
            return total + floor.enemy_count;
        }));
    dungeon_density_after_ = dungeon_density_before_;

    const auto handle = manager_->adopt(
        std::make_unique<zone::Zone>(std::move(coverage)));
    const bool borrowed = manager_->with(handle, [&](zone::Zone& loaded) {
        refresh_quests(loaded);
    });
    if (!borrowed) {
        throw std::logic_error{"覆蓋情境 Site 接管後無法借用"};
    }
    site::unload_site_zone(*manager_, handle, region_tiles, coverage_tile_, seed_,
                           region_id_, now(), ruleset_);
}

} // namespace aetheria::runtime
