// 從載入 Site 與 Region 真值建立湧現任務觀測。

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

}  // namespace aetheria::narrative
