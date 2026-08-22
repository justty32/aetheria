#pragma once

// emergent_quest.h：從世界觀測快照長出湧現任務，並結算運糧的真實 Site 需求。
// 任務只描述需求；Region 變更仍由既有 Site ReductionTable 歸約。

#include "core/world/region_tiles.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace aetheria::zone {
struct Zone;
}

namespace aetheria::narrative {

enum class EmergentQuestKind : std::uint8_t {
    FoodDelivery,
    BanditSuppression,
    MissingPerson,
    PrewarIntelligence,
    DungeonExploration,
};

struct CityFoodObservation {
    world::RegionXY coordinate;
    std::string place_name_key;
    std::uint64_t food_yield{};
    std::uint64_t food_required{};
};

struct TileSecurityObservation {
    world::RegionXY coordinate;
    std::string place_name_key;
    std::uint16_t security{};
    std::uint16_t minimum_security{};
};

struct NamedNpcObservation {
    std::uint64_t entity_uid{};
    std::string person_name_key;
    std::string last_seen_place_name_key;
    bool named{};
    bool known_to_player{};
    bool missing{};
};

struct FactionTensionObservation {
    std::uint32_t first_faction{};
    std::uint32_t second_faction{};
    std::string first_name_key;
    std::string second_name_key;
    std::uint16_t grievance{};
    std::uint16_t minimum_grievance{};
    std::uint64_t first_power{};
    std::uint64_t second_power{};
    std::uint16_t maximum_power_gap_percent{};
};

struct DungeonObservation {
    std::uint64_t dungeon_uid{};
    std::string place_name_key;
    bool cleared{};
    std::uint16_t depth{};
    std::uint16_t minimum_depth{};
};

struct NarrativeWorldSnapshot {
    std::vector<CityFoodObservation> cities;
    std::vector<TileSecurityObservation> tiles;
    std::vector<NamedNpcObservation> named_npcs;
    std::vector<FactionTensionObservation> faction_tensions;
    std::vector<DungeonObservation> dungeons;
};

// NarrativeWorldView 只借用權威 Region 與目前載入的 Site；不保存第二份世界狀態。
// faction_tensions 是留給外交真值接線的介面，本輪刻意不擁有也不推導外交狀態。
struct NarrativeWorldView {
    const world::RegionTiles* region_tiles{};
    std::span<const zone::Zone* const> loaded_sites;
    std::span<const FactionTensionObservation> faction_tensions;
};

struct EmergentQuest {
    std::uint64_t id{};
    EmergentQuestKind kind{EmergentQuestKind::FoodDelivery};
    std::string title_key;
    std::string description_key;
    std::string subject_name_key;
    world::RegionXY coordinate;
    std::uint64_t target_uid{};
    std::uint64_t observed_value{};
    std::uint64_t required_value{};

    bool operator==(const EmergentQuest&) const = default;
};

struct FoodDeliveryReport {
    std::uint64_t food_before{};
    std::uint64_t food_after{};
    std::uint64_t delivered{};
    std::uint32_t reduction_writes{};
};

struct BanditSuppressionReport {
    std::uint16_t order_before{};
    std::uint16_t order_after{};
    std::uint16_t bandit_pressure_before{};
    std::uint16_t bandit_pressure_after{};
    std::uint32_t reduction_writes{};
};

[[nodiscard]] std::vector<EmergentQuest>
detect_emergent_quests(const NarrativeWorldSnapshot& snapshot);

[[nodiscard]] std::vector<EmergentQuest>
detect_emergent_quests(const NarrativeWorldView& world_view, const rules::Ruleset& ruleset);

// 只改 Site 城市糧倉，再呼叫既有 L2→L1 歸約；沒有任務專用 Region setter。
[[nodiscard]] FoodDeliveryReport complete_food_delivery(
    const EmergentQuest& quest, world::RegionTiles& region_tiles, zone::Zone& live_site);

// 只降低 Site 持久層的盜匪壓力，再經正式歸約列更新 Region city.order。
[[nodiscard]] BanditSuppressionReport complete_bandit_suppression(const EmergentQuest& quest,
                                                                  world::RegionTiles& region_tiles,
                                                                  zone::Zone& live_site,
                                                                  const rules::Ruleset& ruleset);

}  // namespace aetheria::narrative
