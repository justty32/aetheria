#pragma once

// site_build_loop.h 定義 L_FULL 城建逐小時流水線與唯一持久城建狀態 component。

#include "core/rules/ruleset.h"
#include "core/site/site_projection.h"
#include "core/world/region_movement.h"

#include <cstdint>
#include <span>
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

    constexpr bool operator==(const SiteAdvanceReport&) const noexcept = default;
};

// SiteAdvanceTarget 是一次批次推進中借用的 L_FULL Site 與其 Region 落點。
// 呼叫端擁有 target 值與 Site；pipeline 只在呼叫期間借用。
// Site 被卸載／銷毀後 site 指標失效，且同一 Site 不得重複列入一批。
struct SiteAdvanceTarget {
    zone::Zone* site{};
    std::int8_t region_z{};
    world::RegionXY coordinate;
};

// SiteAdvanceResult 是一個 Site 的正規化身分與逐 Site 執行報告。
// SiteBatchAdvanceReport 擁有結果值，呼叫端取得批次報告後再擁有其複本。
// 結果值本身不借用 Site，且按 site_key 遞增排列。
struct SiteAdvanceResult {
    zone::ZoneKey site_key{};
    SiteAdvanceReport report;
};

// SiteBatchAdvanceReport 是一次批次推進的逐 Site 結果與聚合路徑計數。
// 呼叫端擁有回傳值；其中 vector 重配會使既有元素參考失效。
// sites 固定依 ZoneKey 排序；其餘欄位是整批路徑的實際執行計數。
struct SiteBatchAdvanceReport {
    std::vector<SiteAdvanceResult> sites;
    std::uint64_t site_hours_advanced{};
    std::uint64_t site_xun_boundaries{};
    std::uint32_t region_xun_advances{};
    std::uint64_t reduction_writes{};
    std::uint64_t xun_reduction_writes{};
};

void enter_full_site(zone::Zone& site, world::RegionTiles& tiles,
                     world::RegionXY coordinate);
void start_construction(zone::Zone& site, std::string_view definition_id, SiteXY origin,
                        const rules::Ruleset& ruleset);
[[nodiscard]] const CityBuildState& city_build_state(const zone::Zone& site);
[[nodiscard]] CityBuildState& city_build_state(zone::Zone& site);
[[nodiscard]] bool valid_city_build_state(const CityBuildState& state,
                                          const rules::Ruleset& ruleset) noexcept;

// SiteTurnPipeline 依 ZoneKey 正規化一批 L_FULL Site；共同旬界先收齊全部歸約，
// 再透過既有 RegionTurnPipeline 結算一次。單 Site 入口只是一元素批次 wrapper。
class SiteTurnPipeline {
public:
    SiteTurnPipeline(const rules::Ruleset& ruleset, zone::ZoneStore& store)
        : ruleset_{ruleset}, region_turn_{ruleset, store} {}

    [[nodiscard]] SiteAdvanceReport advance_hours(zone::Zone& site, zone::Zone& region,
                                                   std::int8_t region_z,
                                                   world::RegionXY coordinate,
                                                   std::uint32_t hours) const;
    [[nodiscard]] SiteBatchAdvanceReport advance_hours(zone::Zone& region,
                                                       std::span<const SiteAdvanceTarget> sites,
                                                       std::uint32_t hours) const;

private:
    const rules::Ruleset& ruleset_;
    world::RegionTurnPipeline region_turn_;
};

}  // namespace aetheria::site
