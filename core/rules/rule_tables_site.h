#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "core/rules/def_types.h"

#include "core/rules/rule_tables.h"

namespace aetheria::rules {

// SiteGenerationRules 是城區骨架 S1～S4 的資料驅動限制。
// Ruleset 擁有值，Site 生成器只讀取複本；比例皆為整數百分比。
struct SiteGenerationRules {
    std::uint8_t block_split_depth{};
    std::uint8_t block_cut_min_percent{};
    std::uint8_t block_cut_max_percent{};
    std::uint8_t block_min_extent{};
    std::uint16_t height_noise_amplitude{};
    std::uint16_t max_buildable_slope{};
    std::uint8_t water_inland_reach{};
    bool loaded{};
};

// WildernessGenerationRules 是荒野 W1～W6 的資料驅動密度與有界成本參數。
// 百分比皆為 0～100；Ruleset 擁有值，荒野生成器只讀取複本。
struct WildernessGenerationRules {
    std::uint16_t height_noise_amplitude{};
    std::uint16_t plain_passable_slope{};
    std::uint16_t hills_passable_slope{};
    std::uint16_t mountain_passable_slope{};
    std::uint8_t jitter_cell_extent{};
    std::uint8_t sparse_vegetation_percent{};
    std::uint8_t forest_vegetation_percent{};
    std::uint8_t base_resource_points{};
    std::uint8_t mine_resource_points{};
    std::uint8_t owned_encounter_points{};
    std::uint8_t unowned_encounter_points{};
    std::uint8_t road_traveler_points{};
    std::uint8_t wilderness_portals{};
    std::uint8_t mountain_portals{};
    std::uint8_t ruin_portals{};
    std::uint8_t ruin_keep_min_percent{};
    std::uint8_t ruin_keep_max_percent{};
    bool loaded{};
};

struct FurnitureDef {
    std::string id;
    LocalRoomKind room{LocalRoomKind::Bedroom};
    std::uint8_t minimum{};
    std::uint8_t maximum{};
};

// LocalBuildingRules 是 L3 路線 A 的幾何、邊材質、垂直層與居民統計規則。
struct LocalBuildingRules {
    std::uint8_t house_margin{};
    std::uint8_t house_depth{};
    std::uint8_t house_frontage_min{};
    std::uint8_t house_frontage_max{};
    std::uint8_t room_split_depth{};
    std::uint8_t room_cut_min_percent{};
    std::uint8_t room_cut_max_percent{};
    std::uint8_t room_min_extent{};
    std::uint8_t upper_floor_percent{};
    std::uint8_t cellar_percent{};
    std::uint8_t residents_min{};
    std::uint8_t residents_max{};
    GroundId foundation_ground{};
    EdgeId wall_edge{};
    EdgeId residential_door_edge{};
    EdgeId commercial_door_edge{};
    EdgeId window_edge{};
    bool loaded{};
};

struct SiteZoneQuota {
    SiteFillZone zone{SiteFillZone::Residential};
    SiteQuotaDriver driver{SiteQuotaDriver::Population};
    std::uint32_t units_per_block{};
    std::uint8_t max_percent{};
};

struct BuildingDef {
    std::string id;
    SiteFillZone zone{SiteFillZone::Residential};
    std::uint8_t frontage{};
    std::uint8_t depth{};
    bool landmark{};
    UndergroundKind underground{UndergroundKind::None};
    std::uint8_t underground_depth{};
};

struct FactionLandmarkStyle {
    std::uint16_t faction{};
    std::vector<BuildingDefId> landmarks;
};

struct SiteFortificationRules {
    std::uint16_t double_wall_defense{};
    std::uint16_t tower_defense{};
    std::uint8_t tower_spacing{};
    std::uint16_t moat_defense{};
    std::uint8_t breach_percent_at_full_damage{};
    EdgeId wall_edge{};
    EdgeId gate_edge{};
    EdgeId tower_edge{};
    EdgeId moat_edge{};
};

// SiteFillRules 是 F1～F5 城區填充的資料驅動規則。
struct SiteFillRules {
    std::vector<SiteZoneQuota> quotas;
    std::vector<FactionLandmarkStyle> faction_styles;
    SiteFortificationRules fortification;
    std::uint8_t base_density_percent{};
    std::uint8_t development_density_per_level{};
    std::uint8_t max_density_percent{};
    bool loaded{};
};

}  // namespace aetheria::rules
