// 湧現任務的 Site 需求結算與 Region 歸約。

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

FoodDeliveryReport complete_food_delivery(const EmergentQuest& quest,
                                          world::RegionTiles& region_tiles, zone::Zone& live_site,
                                          const rules::Ruleset& ruleset) {
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
        site::reduce_live_site_xun(region_tiles, quest.coordinate, live_site, ruleset);
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
        site::reduce_live_site_xun(region_tiles, quest.coordinate, live_site, ruleset);
    } catch (...) {
        order.bandit_pressure = pressure_before;
        throw;
    }
    return {*measured_before, *measured_after, pressure_before, order.bandit_pressure, 1};
}

}  // namespace aetheria::narrative
