// playable_session.cpp：最小可玩情境的權威 core 編排。

#include "core/runtime/playable_session.h"
#include "core/runtime/session_persistence.h"

#include "core/local/dungeon.h"
#include "core/local/local_fov.h"
#include "core/local/local_materialize.h"
#include "core/local/local_movement.h"
#include "core/local/local_reduction.h"
#include "core/narrative/emergent_quest.h"
#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/zone_codec.h"
#include "core/site/site_build_loop.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "core/world/faction_ai.h"
#include "core/worldgen/region_generator.h"
#include "core/worldgen/region_seed.h"

#include <aetheria/runtime/cross_zone.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace aetheria::runtime {
namespace {

[[nodiscard]] rules::CombatModifiers neutral_modifiers(
    const rules::CombatRules& rules) noexcept {
    return {rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
            rules.modifier_scale, rules.modifier_scale};
}

[[nodiscard]] std::uint8_t action_value(ai::FactionActionKind action) noexcept {
    return static_cast<std::uint8_t>(action);
}

[[nodiscard]] std::uint32_t loss_basis_points(std::int32_t loss,
                                              std::int32_t power) noexcept {
    if (power <= 0 || loss <= 0) {
        return 0;
    }
    return static_cast<std::uint32_t>(std::min<std::int64_t>(
        world::kFateBasisPoints,
        static_cast<std::int64_t>(loss) * world::kFateBasisPoints / power));
}

[[nodiscard]] constexpr std::size_t site_index(site::SiteXY tile) noexcept {
    return static_cast<std::size_t>(tile.y) * site::kSiteWidth + tile.x;
}

[[nodiscard]] constexpr std::size_t local_index(local::LocalXY tile) noexcept {
    return static_cast<std::size_t>(tile.y) * local::kLocalWidth + tile.x;
}

[[nodiscard]] bool covers(site::SiteXY tile,
                          const site::ProceduralBuilding& building) noexcept {
    return tile.x >= building.origin.x && tile.y >= building.origin.y &&
           tile.x < static_cast<std::uint32_t>(building.origin.x) + building.width &&
           tile.y < static_cast<std::uint32_t>(building.origin.y) + building.height;
}

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

[[nodiscard]] local::DoorStateQuery door_query(bool open) {
    return [open](const local::LocalEdgeAddress&) {
        return open ? local::DoorState::Open : local::DoorState::Closed;
    };
}

} // namespace

PlayableSession::PlayableSession(std::uint64_t seed, std::uint32_t region_id,
                                 std::string data_directory, zone::ZoneStore& store)
    : seed_{seed}, region_id_{region_id},
      ruleset_{rules::RulesetLoader::load(data_directory)}, store_{store},
      turn_pipeline_{ruleset_, store_} {
    if (store_.manifest().has_value() || !store_.stored_keys().empty()) {
        throw std::logic_error{"new_game 需要空的 ZoneStore"};
    }
    manager_ = std::make_unique<zone::ZoneManager>(
        store_, [this](zone::ZoneKey key, std::unique_ptr<zone::Zone> persistent) {
            return materialize_session_zone(key, std::move(persistent));
        });
    initialize_scenario();
    initialize_diplomacy();
}

PlayableSession::PlayableSession(LoadTag, std::string data_directory,
                                 zone::ZoneStore& store)
    : ruleset_{rules::RulesetLoader::load(data_directory)}, store_{store},
      turn_pipeline_{ruleset_, store_} {
    const auto metadata = inspect_session_save(store_);
    seed_ = metadata.world_seed;
    region_id_ = metadata.region_id;
    region_key_ = metadata.region_key;
    manager_ = std::make_unique<zone::ZoneManager>(
        store_, [this](zone::ZoneKey key, std::unique_ptr<zone::Zone> persistent) {
            return materialize_session_zone(key, std::move(persistent));
        });
    initialize_loaded_scenario();
    if (now() != metadata.now) {
        throw std::runtime_error{"manifest now 與 Region TurnClock 不一致"};
    }
}

std::unique_ptr<PlayableSession>
PlayableSession::load(std::string data_directory, zone::ZoneStore& store) {
    return std::unique_ptr<PlayableSession>{
        new PlayableSession{LoadTag{}, std::move(data_directory), store}};
}

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

std::unique_ptr<zone::Zone> PlayableSession::materialize_session_zone(
    zone::ZoneKey key, std::unique_ptr<zone::Zone> persistent) {
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    if (key == battle_site_key_) {
        if (persistent == nullptr) {
            return std::make_unique<zone::Zone>(site::materialize_site_zone(
                region_tiles, battle_tile_, seed_, region_id_, ruleset_));
        }
        return std::make_unique<zone::Zone>(site::rematerialize_site_zone(
            std::move(*persistent), region_tiles, battle_tile_, seed_, region_id_,
            now(), ruleset_));
    }
    if (key == coverage_site_key_) {
        if (persistent == nullptr) {
            auto result = std::make_unique<zone::Zone>(site::materialize_site_zone(
                region_tiles, coverage_tile_, seed_, region_id_, ruleset_));
            install_coverage_dungeon_entrance(
                std::get<zone::SitePayload>(result->payload).layers,
                local_entrance_, ruleset_);
            return result;
        }
        auto result = std::make_unique<zone::Zone>(site::rematerialize_site_zone(
            std::move(*persistent), region_tiles, coverage_tile_, seed_, region_id_,
            now(), ruleset_));
        install_coverage_dungeon_entrance(
            std::get<zone::SitePayload>(result->payload).layers,
            local_entrance_, ruleset_);
        return result;
    }
    if (key != coverage_local_key_) {
        return nullptr;
    }

    std::optional<zone::Zone> generated;
    const auto site_handle = manager_->get(coverage_site_key_);
    if (!site_handle.has_value()) {
        throw std::logic_error{"具現化 Local 前必須先載入父 Site"};
    }
    const bool borrowed = manager_->with(*site_handle, [&](const zone::Zone& parent) {
        const auto& parent_layer =
            std::get<zone::SitePayload>(parent.payload).layers.procedural;
        const auto feature = region_tiles.feature[region_tiles.index_of(coverage_tile_)];
        generated.emplace(local::materialize_local_zone(
            coverage_site_key_, parent_layer, local_entrance_,
            site::derive_site_seed(seed_, region_id_, coverage_tile_.x,
                                   coverage_tile_.y),
            feature, ruleset_));
    });
    if (!borrowed || !generated.has_value()) {
        throw std::logic_error{"父 Site 未能產生 Local"};
    }

    auto& generated_payload = std::get<zone::LocalPayload>(generated->payload);
    const auto door = ruleset_.find_edge("edge.house_door");
    if (!door.has_value()) {
        throw std::runtime_error{"可玩覆蓋情境缺少門 edge def"};
    }
    auto& ground = generated_payload.layers.at(0);
    const local::LocalXY from{31, 32};
    const local::LocalXY to{32, 32};
    ground.edges[local_index(from) * 4U +
                 static_cast<std::size_t>(spatial::BoundarySide::East)] = *door;
    ground.edges[local_index(to) * 4U +
                 static_cast<std::size_t>(spatial::BoundarySide::West)] = *door;

    if (persistent == nullptr) {
        return std::make_unique<zone::Zone>(std::move(*generated));
    }
    auto& persistent_payload = std::get<zone::LocalPayload>(persistent->payload);
    persistent_payload.layers = std::move(generated_payload.layers);
    persistent->lod = zone::LodLevel::Full;
    return persistent;
}

void PlayableSession::bind_loaded_zone(zone::ZoneHandle handle,
                                       zone::Zone*& destination,
                                       std::string_view description) {
    const bool borrowed = manager_->with(handle, [&](zone::Zone& loaded) {
        loaded.pinned = true;
        destination = &loaded;
    });
    if (!borrowed || destination == nullptr) {
        throw std::runtime_error{"無法綁定已載入 zone：" + std::string{description}};
    }
}

void PlayableSession::rebuild_army_handles() {
    armies_.clear();
    bool found_player{};
    for (const auto entity :
         region_->reg.view<const world::StableId, const world::ArmyState,
                           const world::RegionPosition>()) {
        const auto id = region_->reg.get<const world::StableId>(entity);
        const auto& state = region_->reg.get<const world::ArmyState>(entity);
        armies_.push_back({id, entity});
        if (state.player_controlled) {
            if (found_player) {
                throw std::runtime_error{"存檔含多個玩家控制部隊"};
            }
            player_army_id_ = id;
            found_player = true;
        } else if (id == world::StableId{2001}) {
            enemy_army_id_ = id;
        }
    }
    std::ranges::sort(armies_, {}, [](const PlayableArmy& army) {
        return army.id.uid;
    });
    if (armies_.empty() || !found_player ||
        std::ranges::none_of(armies_, [&](const auto& army_handle) {
            return army_handle.id == player_army_id_;
        }) ||
        std::ranges::none_of(armies_, [&](const auto& army_handle) {
            return army_handle.id == enemy_army_id_;
        })) {
        throw std::runtime_error{"存檔缺少可玩情境所需的玩家或敵方部隊"};
    }
}

void PlayableSession::initialize_loaded_scenario() {
    battle_site_key_ = zone::child_key(region_key_, battle_tile_.x, battle_tile_.y);
    coverage_site_key_ =
        zone::child_key(region_key_, coverage_tile_.x, coverage_tile_.y);
    coverage_local_key_ = zone::child_key(coverage_site_key_, local_entrance_.x,
                                          local_entrance_.y);
    for (const auto [key, description] :
         std::array{std::pair{region_key_, std::string_view{"Region"}},
                    std::pair{battle_site_key_, std::string_view{"戰鬥 Site"}},
                    std::pair{coverage_site_key_, std::string_view{"覆蓋 Site"}}}) {
        if (!store_.contains(key)) {
            throw std::runtime_error{"存檔缺少必要 zone：" + std::string{description}};
        }
    }

    bind_loaded_zone(manager_->require(region_key_), region_, "Region");
    const auto battle = manager_->acquire(battle_site_key_);
    if (!battle.has_value()) {
        throw std::runtime_error{"無法從存檔重展開戰鬥 Site"};
    }
    bind_loaded_zone(*battle, battle_site_, "戰鬥 Site");

    const auto root = manager_->get(zone::kRootZone);
    const bool root_bound = root.has_value() && manager_->with(*root, [&](zone::Zone& loaded) {
        if (loaded.diplomacy.has_value()) {
            diplomacy_ = &*loaded.diplomacy;
        }
    });
    if (!root_bound || diplomacy_ == nullptr) {
        throw std::runtime_error{"存檔 root 缺少外交狀態"};
    }
    rebuild_army_handles();

    const auto coverage = manager_->acquire(coverage_site_key_);
    if (!coverage.has_value()) {
        throw std::runtime_error{"無法從存檔重展開覆蓋 Site"};
    }
    const bool refreshed = manager_->with(*coverage, [&](zone::Zone& loaded) {
        refresh_quests(loaded);
    });
    if (!refreshed) {
        throw std::runtime_error{"冷讀後無法重算湧現任務"};
    }
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    site::unload_site_zone(*manager_, *coverage, region_tiles, coverage_tile_, seed_,
                           region_id_, now(), ruleset_);
    append_event(PlayableEventKind::NewGame, player_start_, region_id_, seed_);
}

zone::ZoneHandle PlayableSession::acquire_coverage_site() {
    const auto acquired = manager_->acquire(coverage_site_key_);
    if (!acquired.has_value()) {
        throw std::runtime_error{"ZoneManager::acquire 未能重展開覆蓋 Site"};
    }
    return *acquired;
}

zone::ZoneHandle PlayableSession::acquire_coverage_local() {
    static_cast<void>(acquire_coverage_site());
    const auto acquired = manager_->acquire(coverage_local_key_);
    if (!acquired.has_value()) {
        throw std::runtime_error{"ZoneManager::acquire 未能重展開覆蓋 Local"};
    }
    return *acquired;
}

void PlayableSession::refresh_quests(zone::Zone& site_zone) {
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    const std::array<const zone::Zone*, 1> loaded_sites{&site_zone};
    quests_ = narrative::detect_emergent_quests(
        {.region_tiles = &region_tiles,
         .loaded_sites = loaded_sites,
         .faction_tensions = {}},
        ruleset_);
    if (accepted_quest_id_.has_value() &&
        std::ranges::none_of(quests_, [&](const auto& quest) {
            return quest.id == *accepted_quest_id_;
        })) {
        accepted_quest_id_.reset();
    }
}

void PlayableSession::initialize_diplomacy() {
    const auto root = manager_->get(zone::kRootZone);
    if (!root.has_value()) {
        throw std::logic_error{"ZoneManager 缺少既有 root"};
    }
    const bool initialized = manager_->with(*root, [&](zone::Zone& loaded) {
        if (loaded.diplomacy.has_value()) {
            throw std::logic_error{"新遊戲 root 已有外交狀態"};
        }
        loaded.diplomacy.emplace(3, seed_, ruleset_);
        diplomacy_ = &*loaded.diplomacy;
    });
    if (!initialized || diplomacy_ == nullptr) {
        throw std::logic_error{"無法在 ZoneManager root 建立外交狀態"};
    }
    diplomacy_->set_faction_truth(world::FactionId{1}, 120'000, 80'000);
    diplomacy_->set_faction_truth(world::FactionId{2}, 48'000, 42'000);
    diplomacy_->set_faction_truth(world::FactionId{3}, 60'000, 70'000);
    for (std::uint16_t observer = 1; observer <= 3; ++observer) {
        for (std::uint16_t target = 1; target <= 3; ++target) {
            if (observer != target) {
                diplomacy_->observe_faction(world::FactionId{observer},
                                            world::FactionId{target}, 0,
                                            time::Tick{0}, 4);
            }
        }
    }
    world::set_managed_faction_goal(*diplomacy_, world::FactionId{2},
                                    ai::FactionGoal::Conquer);
}

void PlayableSession::enter_site() {
    if (residence_ != PlayableResidence::Region) {
        throw std::logic_error{"只有 Region 駐留可親自進入 Site"};
    }
    const auto handle = acquire_coverage_site();
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    const bool borrowed = manager_->with(handle, [&](zone::Zone& loaded) {
        site::enter_full_site(loaded, region_tiles, coverage_tile_);
    });
    if (!borrowed) {
        throw std::logic_error{"進入 Site 時 zone 已不在 manager"};
    }
    residence_ = PlayableResidence::Site;
    ++revision_;
    append_event(PlayableEventKind::SiteEntered, coverage_tile_);
}

void PlayableSession::leave_site() {
    if (residence_ != PlayableResidence::Site) {
        throw std::logic_error{"只有 Site 駐留可返回 Region"};
    }
    const auto handle = manager_->get(coverage_site_key_);
    if (!handle.has_value()) {
        throw std::logic_error{"返回 Region 時 Site 未載入"};
    }
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    const bool borrowed = manager_->with(*handle, [&](zone::Zone& loaded) {
        refresh_quests(loaded);
    });
    if (!borrowed) {
        throw std::logic_error{"返回 Region 前無法讀取 Site"};
    }
    site::unload_site_zone(*manager_, *handle, region_tiles, coverage_tile_, seed_,
                           region_id_, now(), ruleset_);
    residence_ = PlayableResidence::Region;
    ++revision_;
    append_event(PlayableEventKind::SiteLeft, coverage_tile_);
}

void PlayableSession::perform_city_build(bool managed) {
    if ((!managed && residence_ != PlayableResidence::Site) ||
        (managed && residence_ != PlayableResidence::Region)) {
        throw std::logic_error{"城建命令的駐留層不符"};
    }
    const auto handle = acquire_coverage_site();
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    site::SiteAdvanceReport report;
    const bool borrowed = manager_->with(handle, [&](zone::Zone& loaded) {
        site::enter_full_site(loaded, region_tiles, coverage_tile_);
        auto& state = site::city_build_state(loaded);
        if (std::ranges::any_of(state.buildings, [](const auto& building) {
                return building.definition_id == "city.house";
            }) ||
            std::ranges::any_of(state.pending, [](const auto& building) {
                return building.definition_id == "city.house";
            })) {
            throw std::logic_error{"示範住宅已經蓋過"};
        }
        const auto id = ruleset_.find_city_building("city.house");
        const auto* definition = id.has_value() ? ruleset_.city_building(*id) : nullptr;
        if (definition == nullptr) {
            throw std::runtime_error{"城建示範缺少 city.house"};
        }
        const auto& procedural =
            std::get<zone::SitePayload>(loaded.payload).layers.procedural;
        std::optional<site::SiteXY> origin;
        for (std::uint16_t y = 0; y + definition->height <= site::kSiteHeight &&
                                  !origin.has_value();
             ++y) {
            for (std::uint16_t x = 0; x + definition->width <= site::kSiteWidth;
                 ++x) {
                bool legal = true;
                for (std::uint16_t dy = 0; dy < definition->height && legal; ++dy) {
                    for (std::uint16_t dx = 0; dx < definition->width; ++dx) {
                        const auto index = site_index({static_cast<std::uint16_t>(x + dx),
                                                       static_cast<std::uint16_t>(y + dy)});
                        if (procedural.skeleton.buildable[index] == 0 ||
                            procedural.skeleton.roads[index] != 0) {
                            legal = false;
                            break;
                        }
                    }
                }
                if (legal) {
                    origin = site::SiteXY{x, y};
                    break;
                }
            }
        }
        if (!origin.has_value()) {
            throw std::runtime_error{"城區找不到 2x2 可建位置"};
        }
        last_development_before_ =
            region_tiles.reduction_value<world::DevelopmentLevelReduction>(coverage_tile_);
        site::start_construction(loaded, "city.house", *origin, ruleset_);
        site::SiteTurnPipeline pipeline{ruleset_, store_};
        report = pipeline.advance_hours(loaded, *region_, 0, coverage_tile_,
                                        definition->construction_hours);
        if (report.constructions_completed != 1U) {
            throw std::logic_error{"住宅施工沒有完成恰好一棟"};
        }
        site::reduce_live_site_xun(region_tiles, coverage_tile_, loaded, ruleset_);
        last_development_after_ =
            region_tiles.reduction_value<world::DevelopmentLevelReduction>(coverage_tile_);
        refresh_quests(loaded);
    });
    if (!borrowed) {
        throw std::logic_error{"城建時 Site 未載入"};
    }
    if (managed) {
        site::unload_site_zone(*manager_, handle, region_tiles, coverage_tile_, seed_,
                               region_id_, now(), ruleset_);
        append_event(PlayableEventKind::ManagedActivity, coverage_tile_,
                     last_development_before_, last_development_after_);
    }
    ++revision_;
    append_event(PlayableEventKind::ConstructionCompleted, coverage_tile_,
                 last_development_before_, last_development_after_);
}

void PlayableSession::build_city() { perform_city_build(false); }

void PlayableSession::manage_city() { perform_city_build(true); }

void PlayableSession::accept_bandit_quest() {
    if (residence_ != PlayableResidence::Site) {
        throw std::logic_error{"必須在 Site 接受清剿任務"};
    }
    const auto found = std::ranges::find(quests_, narrative::EmergentQuestKind::BanditSuppression,
                                         &narrative::EmergentQuest::kind);
    if (found == quests_.end()) {
        throw std::logic_error{"目前沒有清剿盜匪需求"};
    }
    accepted_quest_id_ = found->id;
    ++revision_;
    append_event(PlayableEventKind::QuestAccepted, coverage_tile_,
                 static_cast<std::int64_t>(found->id));
}

void PlayableSession::enter_local() {
    if (residence_ != PlayableResidence::Site) {
        throw std::logic_error{"只有 Site 駐留可親自進入 Local"};
    }
    static_cast<void>(acquire_coverage_local());
    residence_ = PlayableResidence::Local;
    local_z_ = 0;
    ++revision_;
    append_event(PlayableEventKind::LocalEntered, coverage_tile_, local_entrance_.x,
                 local_entrance_.y);
}

void PlayableSession::leave_local() {
    if (residence_ != PlayableResidence::Local) {
        throw std::logic_error{"只有 Local 地表可返回 Site"};
    }
    const auto site_handle = manager_->get(coverage_site_key_);
    const auto local_handle = manager_->get(coverage_local_key_);
    if (!site_handle.has_value() || !local_handle.has_value()) {
        throw std::logic_error{"返回 Site 時父子 zone 未同時載入"};
    }
    const std::array handles{*site_handle, *local_handle};
    const bool borrowed = manager_->with_many(
        handles, [&](std::span<zone::Zone* const> zones) {
            auto& site_layers =
                std::get<zone::SitePayload>(zones[0]->payload).layers;
            static_cast<void>(local::reduce_live_local(site_layers, local_entrance_,
                                                       *zones[1]));
        });
    if (!borrowed || !manager_->unload(coverage_local_key_)) {
        throw std::logic_error{"Local 歸約或卸載失敗"};
    }
    residence_ = PlayableResidence::Site;
    local_z_ = 0;
    ++revision_;
    append_event(PlayableEventKind::LocalLeft, coverage_tile_);
}

void PlayableSession::open_door_and_move() {
    if (residence_ != PlayableResidence::Local || local_z_ != 0 ||
        local_door_open_) {
        throw std::logic_error{"開門示範只接受尚未開門的 Local 地表"};
    }
    runtime::CrossZoneRuntime runtime{*manager_};
    const local::LocalLocation position{
        coverage_local_key_, {local_player_x_, local_player_y_}};
    const auto closed = local::assess_exploration_step(
        runtime, ruleset_, position, spatial::BoundarySide::East,
        door_query(false));
    if (closed != local::ExplorationStepResult::MustOpenDoor) {
        throw std::logic_error{"Local 門在關閉狀態未要求開門"};
    }
    local_door_open_ = true;
    const auto opened = local::assess_exploration_step(
        runtime, ruleset_, position, spatial::BoundarySide::East,
        door_query(true));
    if (opened != local::ExplorationStepResult::Allowed) {
        local_door_open_ = false;
        throw std::logic_error{"Local 開門後仍不能移動"};
    }
    ++local_player_x_;
    ++revision_;
    append_event(PlayableEventKind::DoorOpened, coverage_tile_, local_player_x_,
                 local_player_y_);
}

void PlayableSession::perform_bandit_suppression(bool managed) {
    if ((!managed && residence_ != PlayableResidence::Local) ||
        (managed && residence_ != PlayableResidence::Site)) {
        throw std::logic_error{"清剿命令的駐留層不符"};
    }
    const auto quest = accepted_quest_id_.has_value()
                           ? std::ranges::find(quests_, *accepted_quest_id_,
                                               &narrative::EmergentQuest::id)
                           : quests_.end();
    if (quest == quests_.end() ||
        quest->kind != narrative::EmergentQuestKind::BanditSuppression) {
        throw std::logic_error{"必須先接受仍存在的清剿任務"};
    }
    const auto site_handle = acquire_coverage_site();
    const auto local_handle = acquire_coverage_local();
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    const std::array handles{site_handle, local_handle};
    const bool borrowed = manager_->with_many(
        handles, [&](std::span<zone::Zone* const> zones) {
            const auto report = narrative::complete_bandit_suppression(
                *quest, region_tiles, *zones[0], ruleset_);
            last_order_before_ = report.order_before;
            last_order_after_ = report.order_after;
            refresh_quests(*zones[0]);
        });
    if (!borrowed) {
        throw std::logic_error{"清剿時 Site/Local 未同時載入"};
    }
    if (managed) {
        if (!manager_->unload(coverage_local_key_)) {
            throw std::logic_error{"代管清剿後 Local 卸載失敗"};
        }
        append_event(PlayableEventKind::ManagedActivity, coverage_tile_,
                     last_order_before_, last_order_after_);
    }
    ++revision_;
    append_event(PlayableEventKind::QuestCompleted, coverage_tile_,
                 last_order_before_, last_order_after_);
}

void PlayableSession::suppress_bandits() { perform_bandit_suppression(false); }

void PlayableSession::manage_local() { perform_bandit_suppression(true); }

void PlayableSession::enter_dungeon() {
    if (residence_ != PlayableResidence::Local) {
        throw std::logic_error{"只有 Local 駐留可親自下地城"};
    }
    bool has_floor{};
    const auto handle = manager_->get(coverage_local_key_);
    if (handle.has_value()) {
        static_cast<void>(manager_->with(*handle, [&](const zone::Zone& loaded) {
            has_floor = std::get<zone::LocalPayload>(loaded.payload).layers.contains(-1);
        }));
    }
    if (!has_floor) {
        throw std::logic_error{"Local 找不到負 z 地城入口"};
    }
    residence_ = PlayableResidence::Dungeon;
    local_z_ = -1;
    ++revision_;
    append_event(PlayableEventKind::DungeonEntered, coverage_tile_, local_z_);
}

void PlayableSession::leave_dungeon() {
    if (residence_ != PlayableResidence::Dungeon) {
        throw std::logic_error{"目前不在地城"};
    }
    residence_ = PlayableResidence::Local;
    local_z_ = 0;
    ++revision_;
}

void PlayableSession::descend_dungeon() {
    if (residence_ != PlayableResidence::Dungeon) {
        throw std::logic_error{"只有地城駐留可往下層"};
    }
    const auto next = static_cast<std::int8_t>(local_z_ - 1);
    bool exists{};
    const auto handle = manager_->get(coverage_local_key_);
    if (handle.has_value()) {
        static_cast<void>(manager_->with(*handle, [&](const zone::Zone& loaded) {
            exists = std::get<zone::LocalPayload>(loaded.payload).layers.contains(next);
        }));
    }
    if (!exists) {
        throw std::logic_error{"已在地城最深層"};
    }
    local_z_ = next;
    ++revision_;
    append_event(PlayableEventKind::DungeonEntered, coverage_tile_, local_z_);
}

void PlayableSession::perform_dungeon_clear(bool managed) {
    if ((!managed && residence_ != PlayableResidence::Dungeon) ||
        (managed && residence_ != PlayableResidence::Local)) {
        throw std::logic_error{"地城清理命令的駐留層不符"};
    }
    const auto site_handle = acquire_coverage_site();
    const auto local_handle = acquire_coverage_local();
    const std::array handles{site_handle, local_handle};
    const bool borrowed = manager_->with_many(
        handles, [&](std::span<zone::Zone* const> zones) {
            auto& site_persistent =
                std::get<zone::SitePayload>(zones[0]->payload).layers.persistent;
            if (site_persistent.dungeons.empty() ||
                site_persistent.dungeons.front().cleared) {
                throw std::logic_error{"地城已經清空"};
            }
            auto& local_persistent =
                std::get<zone::LocalPayload>(zones[1]->payload).dungeon;
            auto dungeon = local::generate_dungeon(
                seed_ ^ UINT64_C(0xD006E002), site_persistent.dungeons.front(),
                local_persistent, ruleset_);
            dungeon_density_before_ = static_cast<std::uint16_t>(std::accumulate(
                dungeon.floors.begin(), dungeon.floors.end(), 0U,
                [](std::uint32_t total, const auto& floor) {
                    return total + floor.enemy_count;
                }));
            if (!dungeon.floors.empty() && !dungeon.floors.front().traps.empty()) {
                static_cast<void>(local::trigger_trap(
                    dungeon.floors.front().traps.front(), 1001, false,
                    local_persistent, ruleset_));
            }
            local::claim_all_treasure(dungeon, local_persistent);
            static_cast<void>(local::defeat_boss(dungeon.boss, dungeon.uid));
            site_persistent.dungeons.front().cleared = true;
            const auto reduced = local::generate_dungeon(
                seed_ ^ UINT64_C(0xD006E002), site_persistent.dungeons.front(),
                local_persistent, ruleset_);
            dungeon_density_after_ = static_cast<std::uint16_t>(std::accumulate(
                reduced.floors.begin(), reduced.floors.end(), 0U,
                [](std::uint32_t total, const auto& floor) {
                    return total + floor.enemy_count;
                }));
            refresh_quests(*zones[0]);
        });
    if (!borrowed) {
        throw std::logic_error{"清地城時 Site/Local 未同時載入"};
    }
    ++revision_;
    append_event(PlayableEventKind::DungeonCleared, coverage_tile_,
                 dungeon_density_before_, dungeon_density_after_);
    if (managed) {
        append_event(PlayableEventKind::ManagedActivity, coverage_tile_,
                     dungeon_density_before_, dungeon_density_after_);
    }
}

void PlayableSession::clear_dungeon() { perform_dungeon_clear(false); }

void PlayableSession::manage_dungeon() { perform_dungeon_clear(true); }

void PlayableSession::sign_peace_treaty() {
    if (residence_ != PlayableResidence::Region) {
        throw std::logic_error{"外交條約只能在 Region 操作"};
    }
    const auto peace = ruleset_.find_treaty("treaty.peace");
    if (!peace.has_value()) {
        throw std::runtime_error{"外交規則缺少 treaty.peace"};
    }
    const bool exists = std::ranges::any_of(diplomacy_->treaties(), [&](const auto& treaty) {
        return treaty.def == *peace &&
               ((treaty.parties[0] == world::FactionId{1} &&
                 treaty.parties[1] == world::FactionId{2}) ||
                (treaty.parties[0] == world::FactionId{2} &&
                 treaty.parties[1] == world::FactionId{1}));
    });
    if (exists) {
        throw std::logic_error{"和平條約已存在"};
    }
    static_cast<void>(diplomacy_->start_treaty(
        *peace, world::FactionId{1}, world::FactionId{2}, now()));
    ++revision_;
    append_event(PlayableEventKind::TreatySigned, {}, 1, 2);
}

void PlayableSession::measure_site_roundtrips() {
    if (residence_ != PlayableResidence::Region) {
        throw std::logic_error{"Site 往返量測必須從 Region 開始"};
    }
    roundtrip_hashes_.clear();
    for (std::uint32_t index = 0; index < 3U; ++index) {
        enter_site();
        leave_site();
        roundtrip_hashes_.push_back(
            serialize::normalized_state_hash(*region_, ruleset_));
    }
    if (!std::ranges::all_of(roundtrip_hashes_, [&](std::uint64_t hash) {
            return hash == roundtrip_hashes_.front();
        })) {
        throw std::logic_error{"Site 往返三次造成世界雜湊漂移"};
    }
}

std::uint64_t PlayableSession::run_city_economy_sample(
    std::string_view site_bytes, std::string_view region_bytes) const {
    auto sample_site = serialize::decode_zone(site_bytes, ruleset_);
    auto sample_region = serialize::decode_zone(region_bytes, ruleset_);
    auto& sample_tiles =
        std::get<zone::RegionPayload>(sample_region->payload).layers.at(0);
    const auto index = sample_tiles.index_of(coverage_tile_);
    sample_tiles.site[index].has_live_site = true;
    sample_tiles.site[index].lod = zone::LodLevel::Full;
    sample_site->lod = zone::LodLevel::Full;
    auto& state = site::city_build_state(*sample_site);
    state.buildings.push_back({"city.workshop", {8, 8}});
    zone::InMemoryZoneStore sample_store{ruleset_};
    site::SiteTurnPipeline pipeline{ruleset_, sample_store};
    const auto report = pipeline.advance_hours(
        *sample_site, *sample_region, 0, coverage_tile_, 240);
    return report.production_produced;
}

void PlayableSession::measure_city_management(std::uint32_t samples) {
    if (residence_ != PlayableResidence::Region || samples == 0U) {
        throw std::logic_error{"城建期望值量測要求 Region 駐留與正樣本數"};
    }
    const auto site_handle = acquire_coverage_site();
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    std::string site_bytes;
    const bool borrowed = manager_->with(site_handle, [&](zone::Zone& loaded) {
        site::enter_full_site(loaded, region_tiles, coverage_tile_);
        site_bytes = serialize::encode_zone(loaded, ruleset_);
    });
    if (!borrowed) {
        throw std::logic_error{"城建期望值量測無法取得 Site"};
    }
    const auto region_bytes = serialize::encode_zone(*region_, ruleset_);
    site::unload_site_zone(*manager_, site_handle, region_tiles, coverage_tile_, seed_,
                           region_id_, now(), ruleset_);

    calibration_n_ = samples;
    calibration_manual_total_ = 0;
    calibration_managed_total_ = 0;
    for (std::uint32_t index = 0; index < samples; ++index) {
        calibration_manual_total_ +=
            run_city_economy_sample(site_bytes, region_bytes);
        calibration_managed_total_ +=
            run_city_economy_sample(site_bytes, region_bytes);
    }
    if (calibration_managed_total_ == 0U) {
        throw std::logic_error{"城建期望值校準分母為 0"};
    }
    calibration_signed_error_percent_ =
        (static_cast<double>(calibration_manual_total_) -
         static_cast<double>(calibration_managed_total_)) /
        static_cast<double>(calibration_managed_total_) * 100.0;
    ++revision_;
}

world::RegionPosition& PlayableSession::position_of(world::StableId unit) {
    for (const auto entity :
         region_->reg.view<const world::StableId, world::RegionPosition>()) {
        if (region_->reg.get<const world::StableId>(entity) == unit) {
            return region_->reg.get<world::RegionPosition>(entity);
        }
    }
    throw std::invalid_argument{"找不到指定的 Region 部隊"};
}

const world::RegionPosition&
PlayableSession::position_of(world::StableId unit) const {
    for (const auto entity :
         region_->reg.view<const world::StableId, const world::RegionPosition>()) {
        if (region_->reg.get<const world::StableId>(entity) == unit) {
            return region_->reg.get<const world::RegionPosition>(entity);
        }
    }
    throw std::invalid_argument{"找不到指定的 Region 部隊"};
}

world::ArmyState& PlayableSession::army(world::StableId unit) {
    const auto found =
        std::ranges::find(armies_, unit, &PlayableArmy::id);
    if (found == armies_.end()) {
        throw std::invalid_argument{"找不到指定的部隊資料"};
    }
    return region_->reg.get<world::ArmyState>(found->entity);
}

const world::ArmyState& PlayableSession::army(world::StableId unit) const {
    const auto found =
        std::ranges::find(armies_, unit, &PlayableArmy::id);
    if (found == armies_.end()) {
        throw std::invalid_argument{"找不到指定的部隊資料"};
    }
    return region_->reg.get<const world::ArmyState>(found->entity);
}

void PlayableSession::append_event(PlayableEventKind kind, world::RegionXY tile,
                                   std::int64_t value_a,
                                   std::int64_t value_b) {
    events_.push_back({next_event_id_++, kind, tile, value_a, value_b});
}

void PlayableSession::issue_move(world::StableId unit,
                                 world::RegionXY target) {
    if (encounter_tile_) {
        throw std::logic_error{"遭遇尚未處理，不能發布移動命令"};
    }
    if (!army(unit).player_controlled) {
        throw std::invalid_argument{"玩家不能替 NPC 部隊發布移動命令"};
    }
    turn_pipeline_.issue_move(*region_, unit, target);
    ++revision_;
    append_event(PlayableEventKind::MoveIssued, target,
                 static_cast<std::int64_t>(unit.uid));
}

void PlayableSession::detect_encounter() {
    const auto player = position_of(player_army_id_).tile;
    const auto enemy = position_of(enemy_army_id_).tile;
    const auto distance = std::abs(static_cast<std::int32_t>(player.x) - enemy.x) +
                          std::abs(static_cast<std::int32_t>(player.y) - enemy.y);
    if (distance > 0) {
        return;
    }
    encounter_tile_ = player;
    region_->reg.clear<world::RegionMoveCommand>();
    append_event(PlayableEventKind::Encounter, player,
                 army(player_army_id_).power, army(enemy_army_id_).power);
}

PlayableAdvanceReport PlayableSession::advance_xun() {
    if (encounter_tile_) {
        throw std::logic_error{"遭遇尚未處理，不能推進下一旬"};
    }
    PlayableAdvanceReport report;
    const auto player_before = position_of(player_army_id_).tile;
    const auto enemy_before = position_of(enemy_army_id_).tile;
    turn_pipeline_.advance_xun(
        *region_,
        [&](world::TurnStage stage) {
            report.stages.push_back(stage);
            if (stage == world::TurnStage::Encounters) {
                detect_encounter();
            }
        },
        [&](zone::Zone& reducing_region) {
            auto& reducing_tiles =
                std::get<zone::RegionPayload>(reducing_region.payload).layers.at(0);
            site::reduce_live_site_xun(reducing_tiles, battle_tile_, *battle_site_, ruleset_);
        },
        [&](time::Tick tick) {
            for (const auto faction : {world::FactionId{2}, world::FactionId{3}}) {
                const auto ai_report = world::advance_faction_ai_xun(
                    *diplomacy_, faction, {4, 0, true, true, faction == world::FactionId{2}},
                    tick, ruleset_);
                const auto action = action_value(ai_report.decision.command.kind);
                report.ai_actions.push_back(action);
                append_event(PlayableEventKind::FactionAiActed, {},
                             static_cast<std::int64_t>(static_cast<std::uint16_t>(faction)),
                             action);
            }
        });
    const auto player_after = position_of(player_army_id_).tile;
    const auto enemy_after = position_of(enemy_army_id_).tile;
    append_event(PlayableEventKind::XunAdvanced, player_after,
                 static_cast<std::int64_t>(now()));
    if (enemy_after != enemy_before) {
        append_event(PlayableEventKind::EnemyMoved, enemy_after, enemy_before.x,
                     enemy_after.x);
    }
    if (player_after == player_before && enemy_after == enemy_before &&
        !encounter_tile_) {
        append_event(PlayableEventKind::EnemyMoved, enemy_after, enemy_before.x,
                     enemy_after.x);
    }
    ++revision_;
    report.encounter_pending = encounter_tile_.has_value();
    return report;
}

const PlayableBattleReport&
PlayableSession::resolve_encounter(PlayableBattleChoice choice) {
    if (!encounter_tile_) {
        throw std::logic_error{"目前沒有可處理的遭遇"};
    }
    auto& player = army(player_army_id_);
    auto& enemy = army(enemy_army_id_);
    const auto& combat_rules = ruleset_.combat_rules();
    const rules::CombatInput input{
        {player.power, neutral_modifiers(combat_rules), {}, 0},
        {enemy.power, neutral_modifiers(combat_rules), {}, 0},
        combat_rules.default_exponent,
        1,
    };
    const auto region_result =
        rules::resolve_region_combat(input, combat_rules);
    const auto layer = choice == PlayableBattleChoice::CommandSite
                           ? world::CombatLayer::Site
                           : world::CombatLayer::Region;
    world::CombatExecutionCounters counters;
    const auto layer_result = world::resolve_scaled_combat(
        input, combat_rules, layer, next_event_id_, seed_ + revision_, {}, {},
        &counters);
    const auto before = tile_state(*encounter_tile_);
    player.power = std::max(0, player.power - layer_result.loss_a);
    enemy.power = std::max(0, enemy.power - layer_result.loss_b);

    auto& site_layers =
        std::get<zone::SitePayload>(battle_site_->payload).layers;
    if (!site_layers.persistent.buildings.empty()) {
        site_layers.persistent.buildings.front().state =
            site::BuildingState::Idle;
    }
    site_layers.persistent.order = site::SiteOrderState{
        .garrison_coverage = 12,
        .patrol_coverage = 6,
        .bandit_pressure = 28,
        .refugee_pressure = 20,
    };
    auto& region_tiles =
        std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    site::reduce_live_site_xun(region_tiles, *encounter_tile_, *battle_site_, ruleset_);

    world::NamedFateLedger ledger;
    ledger.members.push_back({
        .entity_uid = 9001,
        .cohort_id = enemy_army_id_.uid,
        .name_key = "守軍隊長艾琳",
        .significance = world::Significance::Site,
        .significance_reason = "battle.commander",
        .modifiers = {},
        .marked = true,
    });
    const auto stage_one = world::FateResolver::apply_stage_one(
        region_tiles, *encounter_tile_,
        {.event_id = next_event_id_,
         .cohort_id = enemy_army_id_.uid,
         .site_key = zone::value_of(battle_site_->key),
         .base_loss_basis_points = loss_basis_points(layer_result.loss_b,
                                                     input.side_b.power),
         .relief_basis_points = 0,
         .occurred_at = now(),
         .place_key = "暮橋鎮"});
    world::FateExecutionCounters fate_counters;
    const auto fate = world::FateResolver::resolve_present(
        ledger, stage_one, {}, fate_counters);
    const auto named_outcome = fate.decisions.empty()
                                   ? world::FateOutcome::Unharmed
                                   : fate.decisions.front().outcome;

    const bool player_won =
        region_result.outcome == rules::Outcome::SideBRouted ||
        layer_result.loss_b >= layer_result.loss_a;
    if (player_won) {
        region_tiles.owner[region_tiles.index_of(*encounter_tile_)] =
            world::FactionId{1};
    }
    const auto after = tile_state(*encounter_tile_);
    battle_report_ = PlayableBattleReport{
        choice, region_result, layer_result, *encounter_tile_, before, after,
        named_outcome, "守軍隊長艾琳"};
    append_event(PlayableEventKind::BattleResolved, *encounter_tile_,
                 layer_result.loss_a, layer_result.loss_b);
    append_event(PlayableEventKind::WorldChanged, *encounter_tile_,
                 before.population, after.population);
    encounter_tile_.reset();
    ++revision_;
    return *battle_report_;
}

const world::RegionTiles& PlayableSession::tiles() const {
    return std::get<zone::RegionPayload>(region_->payload).layers.at(0);
}

std::vector<PlayableArmyView> PlayableSession::armies() const {
    std::vector<PlayableArmyView> result;
    result.reserve(armies_.size());
    for (const auto& value : armies_) {
        const auto& state = region_->reg.get<const world::ArmyState>(value.entity);
        result.push_back({value.id, state.faction, position_of(value.id).tile,
                          state.power, state.player_controlled});
    }
    return result;
}

PlayableTileState PlayableSession::tile_state(world::RegionXY tile) const {
    const auto& region_tiles = tiles();
    const auto index = region_tiles.index_of(tile);
    return {region_tiles.owner[index],
            region_tiles.reduction_value<world::PopulationReduction>(tile),
            region_tiles.reduction_value<world::OrderReduction>(tile)};
}

PlayableCoverageSummary PlayableSession::coverage_summary() const {
    const auto& region_tiles = tiles();
    PlayableCoverageSummary result;
    result.residence = residence_;
    result.development =
        region_tiles.reduction_value<world::DevelopmentLevelReduction>(coverage_tile_);
    result.order = region_tiles.reduction_value<world::OrderReduction>(coverage_tile_);
    result.production =
        region_tiles.reduction_value<world::ProductionStockReduction>(coverage_tile_);
    result.quest_count = static_cast<std::uint32_t>(quests_.size());
    result.bandit_quest_available =
        std::ranges::any_of(quests_, [](const auto& quest) {
            return quest.kind == narrative::EmergentQuestKind::BanditSuppression;
        });
    result.bandit_quest_accepted = accepted_quest_id_.has_value();
    result.dungeon_quest_available =
        std::ranges::any_of(quests_, [](const auto& quest) {
            return quest.kind == narrative::EmergentQuestKind::DungeonExploration;
        });
    result.dungeon_cleared = false;

    const auto inspect_site = [&](const zone::Zone& site_zone) {
        const auto states = site_zone.reg.view<const site::CityBuildState>();
        if (!states.empty()) {
            result.city_buildings = static_cast<std::uint32_t>(
                states.get<const site::CityBuildState>(*states.begin()).buildings.size());
        }
        const auto& persistent =
            std::get<zone::SitePayload>(site_zone.payload).layers.persistent;
        result.dungeon_cleared = !persistent.dungeons.empty() &&
                                 persistent.dungeons.front().cleared;
        result.persistent_buildings = static_cast<std::uint32_t>(persistent.buildings.size());
        result.persistent_population =
            *site::ReductionTable::reduce(std::get<zone::SitePayload>(site_zone.payload).layers)
                 .value<world::PopulationReduction>();
    };
    if (const auto loaded = manager_->get(coverage_site_key_)) {
        static_cast<void>(manager_->with(*loaded, inspect_site));
    } else if (const auto stored = store_.load(coverage_site_key_)) {
        inspect_site(*stored);
    }

    result.dungeon_density_before = dungeon_density_before_;
    result.dungeon_density_after = dungeon_density_after_;
    result.treaty_count = static_cast<std::uint32_t>(diplomacy_->treaties().size());
    result.last_order_before = last_order_before_;
    result.last_order_after = last_order_after_;
    result.last_development_before = last_development_before_;
    result.last_development_after = last_development_after_;
    result.roundtrip_hashes = roundtrip_hashes_;
    result.calibration_n = calibration_n_;
    result.manual_total = calibration_manual_total_;
    result.managed_total = calibration_managed_total_;
    result.signed_relative_error_percent = calibration_signed_error_percent_;
    return result;
}

std::optional<PlayableGridView> PlayableSession::site_view() const {
    const auto handle = manager_->get(coverage_site_key_);
    if (!handle.has_value()) {
        return std::nullopt;
    }
    PlayableGridView result{site::kSiteWidth, site::kSiteHeight, 0,
                            {}, 0, 0};
    const bool borrowed = manager_->with(*handle, [&](const zone::Zone& loaded) {
        const auto& layers = std::get<zone::SitePayload>(loaded.payload).layers;
        const auto tile_count = layers.procedural.skeleton.ground.size();
        result.cells.assign(tile_count, UINT8_C(0));
        if (tile_count != site::kSiteTileCount) {
            return;
        }
        for (std::size_t index = 0; index < result.cells.size(); ++index) {
            result.cells[index] = layers.procedural.skeleton.roads[index] != 0
                                      ? UINT8_C(1)
                                      : (layers.procedural.skeleton.buildable[index] != 0
                                             ? UINT8_C(2)
                                             : UINT8_C(0));
        }
        for (const auto& building : layers.procedural.buildings) {
            for (std::uint16_t y = building.origin.y;
                 y < static_cast<std::uint32_t>(building.origin.y) + building.height; ++y) {
                for (std::uint16_t x = building.origin.x;
                     x < static_cast<std::uint32_t>(building.origin.x) + building.width; ++x) {
                    result.cells[site_index({x, y})] = UINT8_C(3);
                }
            }
        }
        const auto states = loaded.reg.view<const site::CityBuildState>();
        if (!states.empty()) {
            const auto& state =
                states.get<const site::CityBuildState>(*states.begin());
            for (const auto& building : state.buildings) {
                const auto id = ruleset_.find_city_building(building.definition_id);
                const auto* definition = id.has_value() ? ruleset_.city_building(*id) : nullptr;
                if (definition == nullptr) {
                    continue;
                }
                for (std::uint16_t y = building.origin.y;
                     y < static_cast<std::uint32_t>(building.origin.y) + definition->height;
                     ++y) {
                    for (std::uint16_t x = building.origin.x;
                         x < static_cast<std::uint32_t>(building.origin.x) + definition->width;
                         ++x) {
                        result.cells[site_index({x, y})] = UINT8_C(4);
                    }
                }
            }
        }
        result.cells[site_index(local_entrance_)] = UINT8_C(5);
    });
    return borrowed ? std::optional<PlayableGridView>{std::move(result)}
                    : std::nullopt;
}

std::optional<PlayableGridView> PlayableSession::local_view() const {
    const auto handle = manager_->get(coverage_local_key_);
    if (!handle.has_value()) {
        return std::nullopt;
    }
    PlayableGridView result{local::kLocalWidth, local::kLocalHeight, local_z_,
                            std::vector<std::uint8_t>(local::kLocalTileCount),
                            local_player_x_, local_player_y_};
    std::vector<std::uint8_t> visible(local::kLocalTileCount,
                                      local_z_ == 0 ? UINT8_C(0) : UINT8_C(1));
    if (local_z_ == 0) {
        runtime::CrossZoneRuntime runtime{*manager_};
        const auto fov = local::calculate_fov(
            runtime, ruleset_,
            {coverage_local_key_, {local_player_x_, local_player_y_}}, {12, 12},
            door_query(local_door_open_));
        for (const auto& location : fov.visible) {
            if (location.zone == coverage_local_key_) {
                visible[local_index(location.tile)] = UINT8_C(1);
            }
        }
    }
    const bool borrowed = manager_->with(*handle, [&](const zone::Zone& loaded) {
        const auto& payload = std::get<zone::LocalPayload>(loaded.payload);
        const auto found = payload.layers.find(local_z_);
        if (found == payload.layers.end()) {
            return;
        }
        for (std::size_t index = 0; index < result.cells.size(); ++index) {
            if (visible[index] == 0) {
                result.cells[index] = UINT8_C(0);
            } else if (found->second.overlay[index] == local::OverlayId::Stairs) {
                result.cells[index] = UINT8_C(4);
            } else if (found->second.light[index] != 0) {
                result.cells[index] = UINT8_C(2);
            } else {
                result.cells[index] = UINT8_C(1);
            }
        }
    });
    if (!borrowed) {
        return std::nullopt;
    }
    if (local_z_ == 0) {
        result.cells[local_index({local_player_x_, local_player_y_})] = UINT8_C(5);
        if (!local_door_open_) {
            result.cells[local_index({32, 32})] = UINT8_C(6);
        }
    }
    return result;
}

time::Tick PlayableSession::now() const {
    return world::turn_clock(*region_).now;
}

void PlayableSession::save_game(zone::ZoneStore& destination) {
    if (encounter_tile_.has_value()) {
        throw std::logic_error{"遭遇尚未處理：只能在回合尾端、無 pending 遭遇時存檔"};
    }
    save_session(store_, destination, *manager_, seed_, now());
}

} // namespace aetheria::runtime
