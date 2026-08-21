#pragma once

// site_build_loop.h 定義 L_FULL 城建逐小時流水線與唯一持久城建狀態 component。

#include "core/rules/ruleset.h"
#include "core/site/site_projection.h"
#include "core/world/region_movement.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aetheria::site {

struct CityBuilding {
    std::string definition_id;
    SiteXY origin;

    template <typename Archive> void serialize(Archive& archive) {
        archive(definition_id, origin);
    }
    bool operator==(const CityBuilding&) const = default;
};

struct PendingConstruction {
    std::string definition_id;
    SiteXY origin;
    std::uint16_t remaining_hours{};

    template <typename Archive> void serialize(Archive& archive) {
        archive(definition_id, origin, remaining_hours);
    }
    bool operator==(const PendingConstruction&) const = default;
};

enum class SiteMigrationObjectKind : std::uint8_t {
    PersistentBuilding,
    CityBuilding,
    PendingConstruction,
};

// 毀於骨架變動的物件保留完整可辨識資料，但 former_coordinate 只作歷史記錄，
// 不再是 live Site 上的座標。
struct SiteMigrationDestroyedObject {
    SiteMigrationObjectKind kind{SiteMigrationObjectKind::PersistentBuilding};
    std::string definition_id;
    SiteXY former_coordinate;
    BuildingType persistent_type{BuildingType::SettlementHall};
    BuildingState persistent_state{BuildingState::Active};
    std::uint32_t aging_seconds{};
    std::uint16_t remaining_hours{};

    template <typename Archive> void serialize(Archive& archive) {
        archive(kind, definition_id, former_coordinate, persistent_type, persistent_state,
                aging_seconds, remaining_hours);
    }
    bool operator==(const SiteMigrationDestroyedObject&) const = default;
};

// 敘事事件是 core 對顯示層的玩家可見資料；migration 不得只有靜默座標修正。
struct SiteMigrationEvent {
    std::uint64_t old_skeleton_hash{};
    std::uint64_t new_skeleton_hash{};
    std::uint32_t retained{};
    std::uint32_t relocated{};
    std::uint32_t destroyed{};
    std::string narrative;

    template <typename Archive> void serialize(Archive& archive) {
        archive(old_skeleton_hash, new_skeleton_hash, retained, relocated, destroyed,
                narrative);
    }
    bool operator==(const SiteMigrationEvent&) const = default;
};

struct SiteMigrationHistory {
    std::vector<SiteMigrationDestroyedObject> destroyed_objects;
    std::vector<SiteMigrationEvent> events;

    template <typename Archive> void serialize(Archive& archive) {
        archive(destroyed_objects, events);
    }
    bool operator==(const SiteMigrationHistory&) const = default;
};

struct CityEconomy {
    std::uint32_t population{};
    std::uint64_t food_stock{};
    std::uint64_t production_stock{};
    std::int64_t population_micro_remainder{};
    std::uint16_t hours_into_xun{};
    std::uint8_t satisfaction{};

    template <typename Archive> void serialize(Archive& archive) {
        archive(population, food_stock, production_stock, population_micro_remainder,
                hours_into_xun, satisfaction);
    }
    constexpr bool operator==(const CityEconomy&) const noexcept = default;
};

// CityBuildState 只掛在已進入城建循環的城市 Site；荒野不建立此 component。
struct CityBuildState {
    std::vector<CityBuilding> buildings;
    std::vector<PendingConstruction> pending;
    CityEconomy economy;
    SiteMigrationHistory migration;

    template <typename Archive> void serialize(Archive& archive) {
        archive(buildings, pending, economy, migration);
    }
    bool operator==(const CityBuildState&) const = default;
};

struct SiteAdvanceReport {
    std::uint32_t hours_advanced{};
    std::uint32_t xun_boundaries{};
    std::uint32_t constructions_completed{};
    std::uint32_t completion_reductions{};
    std::uint64_t adjacency_bonus_triggers{};
    std::uint64_t food_produced{};
    std::uint64_t production_produced{};
    std::uint32_t population_births{};
    std::uint32_t population_deaths{};
    std::uint64_t persistent_object_advances{};
    std::uint32_t aging_transitions{};
    bool aging_cap_hit{};
};

void enter_full_site(zone::Zone& site, world::RegionTiles& tiles,
                     world::RegionXY coordinate);
void start_construction(zone::Zone& site, std::string_view definition_id, SiteXY origin,
                        const rules::Ruleset& ruleset);
[[nodiscard]] const CityBuildState& city_build_state(const zone::Zone& site);
[[nodiscard]] CityBuildState& city_build_state(zone::Zone& site);
[[nodiscard]] bool valid_city_build_state(const CityBuildState& state,
                                          const rules::Ruleset& ruleset) noexcept;

// SiteTurnPipeline 每 240 小時透過既有 RegionTurnPipeline 推進一旬；建造完成當下另回填一次歸約。
class SiteTurnPipeline {
public:
    SiteTurnPipeline(const rules::Ruleset& ruleset, zone::ZoneStore& store)
        : ruleset_{ruleset}, region_turn_{ruleset, store} {}

    [[nodiscard]] SiteAdvanceReport advance_hours(zone::Zone& site, zone::Zone& region,
                                                   std::int8_t region_z,
                                                   world::RegionXY coordinate,
                                                   std::uint32_t hours) const;

private:
    const rules::Ruleset& ruleset_;
    world::RegionTurnPipeline region_turn_;
};

}  // namespace aetheria::site
