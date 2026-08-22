// emergent_quest.cpp：五種真實需求的確定性偵測與運糧歸約結算。

#include "core/narrative/emergent_quest.h"

#include "core/site/site_build_loop.h"
#include "core/site/site_reduction.h"
#include "core/zone/zone_key.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>

namespace aetheria::narrative {
namespace {

[[nodiscard]] std::uint64_t quest_id(EmergentQuestKind kind, std::uint64_t target) noexcept {
    return (static_cast<std::uint64_t>(kind) + 1U) * UINT64_C(0x9E3779B97F4A7C15) ^ target;
}

[[nodiscard]] std::uint64_t coordinate_id(world::RegionXY coordinate) noexcept {
    return static_cast<std::uint64_t>(static_cast<std::uint16_t>(coordinate.x)) << 16U |
           static_cast<std::uint16_t>(coordinate.y);
}

[[nodiscard]] bool powers_are_close(const FactionTensionObservation& tension) noexcept {
    const auto larger = std::max(tension.first_power, tension.second_power);
    const auto smaller = std::min(tension.first_power, tension.second_power);
    if (larger == 0) {
        return false;
    }
    const auto gap = larger - smaller;
    if (tension.maximum_power_gap_percent >= 100U) {
        return true;
    }
    const auto allowed_gap =
        larger / 100U * tension.maximum_power_gap_percent +
        (larger % 100U) * tension.maximum_power_gap_percent / 100U;
    return gap <= allowed_gap;
}

}  // namespace

std::vector<EmergentQuest> detect_emergent_quests(const NarrativeWorldSnapshot& snapshot) {
    std::vector<EmergentQuest> result;
    for (const auto& city : snapshot.cities) {
        if (city.food_yield >= city.food_required) {
            continue;
        }
        result.push_back({quest_id(EmergentQuestKind::FoodDelivery,
                                   coordinate_id(city.coordinate)),
                          EmergentQuestKind::FoodDelivery,
                          "quest.food_delivery.title",
                          "quest.food_delivery.description",
                          city.place_name_key,
                          city.coordinate,
                          coordinate_id(city.coordinate),
                          city.food_yield,
                          city.food_required});
    }
    for (const auto& tile : snapshot.tiles) {
        if (tile.security >= tile.minimum_security) {
            continue;
        }
        result.push_back({quest_id(EmergentQuestKind::BanditSuppression,
                                   coordinate_id(tile.coordinate)),
                          EmergentQuestKind::BanditSuppression,
                          "quest.bandit_suppression.title",
                          "quest.bandit_suppression.description",
                          tile.place_name_key,
                          tile.coordinate,
                          coordinate_id(tile.coordinate),
                          tile.security,
                          tile.minimum_security});
    }
    for (const auto& npc : snapshot.named_npcs) {
        if (!npc.named || !npc.known_to_player || !npc.missing) {
            continue;
        }
        result.push_back({quest_id(EmergentQuestKind::MissingPerson, npc.entity_uid),
                          EmergentQuestKind::MissingPerson,
                          "quest.missing_person.title",
                          "quest.missing_person.description",
                          npc.person_name_key,
                          {},
                          npc.entity_uid,
                          1,
                          0});
    }
    for (const auto& tension : snapshot.faction_tensions) {
        if (tension.first_faction == tension.second_faction ||
            tension.grievance < tension.minimum_grievance || !powers_are_close(tension)) {
            continue;
        }
        const auto first = std::min(tension.first_faction, tension.second_faction);
        const auto second = std::max(tension.first_faction, tension.second_faction);
        const auto target = static_cast<std::uint64_t>(first) << 32U | second;
        result.push_back({quest_id(EmergentQuestKind::PrewarIntelligence, target),
                          EmergentQuestKind::PrewarIntelligence,
                          "quest.prewar_intelligence.title",
                          "quest.prewar_intelligence.description",
                          tension.second_name_key,
                          {},
                          target,
                          tension.grievance,
                          tension.minimum_grievance});
    }
    for (const auto& dungeon : snapshot.dungeons) {
        if (dungeon.cleared || dungeon.depth < dungeon.minimum_depth) {
            continue;
        }
        result.push_back({quest_id(EmergentQuestKind::DungeonExploration,
                                   dungeon.dungeon_uid),
                          EmergentQuestKind::DungeonExploration,
                          "quest.dungeon_exploration.title",
                          "quest.dungeon_exploration.description",
                          dungeon.place_name_key,
                          {},
                          dungeon.dungeon_uid,
                          dungeon.depth,
                          dungeon.minimum_depth});
    }
    std::ranges::sort(result, [](const EmergentQuest& left, const EmergentQuest& right) {
        return std::tie(left.kind, left.target_uid, left.coordinate) <
               std::tie(right.kind, right.target_uid, right.coordinate);
    });
    return result;
}

std::vector<EmergentQuest> detect_emergent_quests(const NarrativeWorldView& world_view,
                                                  const rules::Ruleset& ruleset) {
    if (world_view.region_tiles == nullptr || !world_view.region_tiles->valid_layout()) {
        throw std::invalid_argument{"湧現任務要求有效的 Region 世界真值"};
    }
    const auto& quest_rules = ruleset.world_observation_rules();
    if (!quest_rules.loaded) {
        throw std::logic_error{"湧現任務缺少世界觀測資料規則"};
    }

    NarrativeWorldSnapshot snapshot;
    snapshot.faction_tensions.assign(world_view.faction_tensions.begin(),
                                     world_view.faction_tensions.end());
    std::set<std::uint64_t> npc_uids;
    std::set<std::uint64_t> dungeon_uids;
    for (const auto* loaded_site : world_view.loaded_sites) {
        if (loaded_site == nullptr || zone::level_of(loaded_site->key) != zone::ZoneLevel::Site ||
            loaded_site->lod == zone::LodLevel::Absent) {
            throw std::invalid_argument{"湧現任務 world view 含未載入或非 Site 的 zone"};
        }
        const auto coordinate = world::RegionXY{
            static_cast<std::int16_t>(zone::site_x_of(loaded_site->key)),
            static_cast<std::int16_t>(zone::site_y_of(loaded_site->key)),
        };
        const auto index = world_view.region_tiles->index_of(coordinate);
        if (!world_view.region_tiles->site.at(index).has_live_site) {
            throw std::logic_error{"湧現任務 world view 的 Site 未被 Region 標記為 live"};
        }
        const auto* payload = std::get_if<zone::SitePayload>(&loaded_site->payload);
        if (payload == nullptr || !site::valid_persistent_layer(payload->layers.persistent)) {
            throw std::logic_error{"湧現任務 world view 含無效 Site 持久層"};
        }
        const auto& persistent = payload->layers.persistent;

        const auto city_states = loaded_site->reg.view<const site::CityBuildState>();
        if (city_states.size() > 1U) {
            throw std::logic_error{"湧現任務 world view 的 Site 含多份城建真值"};
        }
        if (!city_states.empty()) {
            if (persistent.place_name_key.empty()) {
                throw std::logic_error{"城鎮任務來源缺少持久地名 key"};
            }
            const auto& economy =
                city_states.get<const site::CityBuildState>(*city_states.begin()).economy;
            const auto region_food =
                world_view.region_tiles->reduction_value<world::FoodStockReduction>(coordinate);
            if (region_food != economy.food_stock) {
                throw std::logic_error{"運糧觀測遇到尚未歸約的 Site 糧食狀態"};
            }
            snapshot.cities.push_back({coordinate, persistent.place_name_key, region_food,
                                       quest_rules.food_delivery_required});
        }

        const auto site_order = site::measure_site_order(persistent);
        if (site_order.has_value()) {
            if (persistent.place_name_key.empty()) {
                throw std::logic_error{"治安觀測來源缺少持久地名 key"};
            }
            const auto region_order =
                world_view.region_tiles->reduction_value<world::OrderReduction>(coordinate);
            if (region_order != *site_order) {
                throw std::logic_error{"治安觀測遇到尚未歸約的 Site 治安狀態"};
            }
            snapshot.tiles.push_back({coordinate, persistent.place_name_key, region_order,
                                      quest_rules.bandit_minimum_order});
        }

        for (const auto& npc : persistent.named_npcs) {
            if (!npc_uids.insert(npc.uid).second) {
                throw std::logic_error{"湧現任務 world view 含重複具名 NPC uid"};
            }
            snapshot.named_npcs.push_back({npc.uid, npc.person_name_key,
                                           npc.last_seen_place_name_key, true, npc.known_to_player,
                                           npc.missing});
        }
        for (const auto& dungeon : persistent.dungeons) {
            if (!dungeon_uids.insert(dungeon.uid).second) {
                throw std::logic_error{"湧現任務 world view 含重複地城 uid"};
            }
            snapshot.dungeons.push_back({dungeon.uid, dungeon.place_name_key, dungeon.cleared,
                                         dungeon.depth, quest_rules.dungeon_minimum_depth});
        }
    }
    return detect_emergent_quests(snapshot);
}

FoodDeliveryReport complete_food_delivery(const EmergentQuest& quest,
                                          world::RegionTiles& region_tiles,
                                          zone::Zone& live_site) {
    if (quest.kind != EmergentQuestKind::FoodDelivery ||
        quest.observed_value >= quest.required_value) {
        throw std::invalid_argument{"完成運糧只接受仍有真實缺口的運糧任務"};
    }
    const auto region_before =
        region_tiles.reduction_value<world::FoodStockReduction>(quest.coordinate);
    auto& economy = site::city_build_state(live_site).economy;
    if (economy.food_stock != quest.observed_value || region_before != quest.observed_value) {
        throw std::logic_error{"運糧任務觀測已過期，拒絕改錯城市或覆蓋新狀態"};
    }
    const auto delivered = quest.required_value - quest.observed_value;
    if (economy.food_stock > std::numeric_limits<std::uint64_t>::max() - delivered) {
        throw std::overflow_error{"運糧任務使城市糧食溢位"};
    }
    economy.food_stock += delivered;
    try {
        site::reduce_live_site_xun(region_tiles, quest.coordinate, live_site);
    } catch (...) {
        economy.food_stock -= delivered;
        throw;
    }
    return {region_before,
            region_tiles.reduction_value<world::FoodStockReduction>(quest.coordinate),
            delivered,
            1};
}

BanditSuppressionReport complete_bandit_suppression(const EmergentQuest& quest,
                                                    world::RegionTiles& region_tiles,
                                                    zone::Zone& live_site,
                                                    const rules::Ruleset& ruleset) {
    const auto& quest_rules = ruleset.world_observation_rules();
    if (quest.kind != EmergentQuestKind::BanditSuppression ||
        quest.observed_value >= quest.required_value || !quest_rules.loaded ||
        quest.required_value != quest_rules.bandit_minimum_order) {
        throw std::invalid_argument{"完成清剿只接受仍低於資料門檻的清剿任務"};
    }
    auto* payload = std::get_if<zone::SitePayload>(&live_site.payload);
    if (payload == nullptr || !payload->layers.persistent.order.has_value()) {
        throw std::logic_error{"清剿任務找不到 Site 持久治安因子"};
    }
    auto& order = *payload->layers.persistent.order;
    const auto measured_before = site::measure_site_order(payload->layers.persistent);
    const auto region_before =
        region_tiles.reduction_value<world::OrderReduction>(quest.coordinate);
    if (!measured_before.has_value() || region_before != *measured_before ||
        region_before != quest.observed_value) {
        throw std::logic_error{"清剿任務觀測已過期，拒絕覆蓋新的治安狀態"};
    }
    const auto pressure_before = order.bandit_pressure;
    const auto removed = std::min(order.bandit_pressure, quest_rules.bandit_pressure_reduction);
    if (removed == 0) {
        throw std::logic_error{"清剿任務沒有可降低的盜匪壓力"};
    }
    order.bandit_pressure = static_cast<std::uint16_t>(order.bandit_pressure - removed);
    const auto measured_after = site::measure_site_order(payload->layers.persistent);
    if (!measured_after.has_value() || *measured_after <= *measured_before) {
        order.bandit_pressure = pressure_before;
        throw std::logic_error{"清剿完成未能提高 Site 治安"};
    }
    try {
        site::reduce_live_site_xun(region_tiles, quest.coordinate, live_site);
    } catch (...) {
        order.bandit_pressure = pressure_before;
        throw;
    }
    return {*measured_before, *measured_after, pressure_before, order.bandit_pressure, 1};
}

}  // namespace aetheria::narrative
