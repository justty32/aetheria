// emergent_quest.cpp：五種真實需求的確定性偵測與運糧歸約結算。

#include "core/narrative/emergent_quest.h"

#include "core/site/site_build_loop.h"
#include "core/site/site_reduction.h"

#include <algorithm>
#include <limits>
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

}  // namespace aetheria::narrative
