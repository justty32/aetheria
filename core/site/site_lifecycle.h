#pragma once

// site_lifecycle.h 定義 L_ABSENT 的 SiteDigest 與持久層閉式時間補算。

#include "core/site/site_build_loop.h"
#include "core/time/tick.h"

#include <cstdint>
#include <vector>

namespace aetheria::site {

inline constexpr time::Duration kBuildingAgingCap = time::kXun * 24;
inline constexpr time::Duration kBuildingAgingStep = time::kXun * 6;

// SiteDigest 是卸載 Site 的完整持久摘要；程序層與易失層不在此型別中。
struct SiteDigest {
    time::Tick unload_tick{};
    std::uint64_t site_seed{};
    std::uint64_t skeleton_hash{};
    std::vector<PersistentBuilding> objects;
    std::vector<CityBuilding> city_buildings;
    std::vector<PendingConstruction> pending;
    CityEconomy economy;
    std::vector<std::uint64_t> story_flags;
    SiteMigrationHistory migration;

    template <typename Archive> void serialize(Archive& archive) {
        auto raw_tick = static_cast<std::int64_t>(unload_tick);
        archive(raw_tick, site_seed, skeleton_hash, objects, city_buildings, pending, economy,
                story_flags, migration);
        unload_tick = time::Tick{raw_tick};
    }
    bool operator==(const SiteDigest&) const = default;
};

struct SiteMigrationReport {
    bool applied{};
    std::uint32_t retained{};
    std::uint32_t relocated{};
    std::uint32_t destroyed{};
    std::uint32_t events_generated{};
};

struct SiteCatchUpReport {
    std::uint64_t elapsed_seconds{};
    std::uint64_t aging_seconds_applied{};
    std::uint32_t pending_advanced{};
    std::uint32_t constructions_completed{};
    std::uint32_t persistent_objects_advanced{};
    std::uint32_t aging_transitions{};
    bool aging_cap_hit{};
    SiteMigrationReport migration;
};

[[nodiscard]] bool valid_site_digest(const SiteDigest& digest,
                                     const rules::Ruleset& ruleset) noexcept;

// 對 live 持久物件推進 elapsed；L_FULL 可小步呼叫，重載可一次閉式呼叫。
[[nodiscard]] SiteCatchUpReport advance_persistent_objects(
    SitePersistentLayer& persistent, const SiteFastVars& fast, time::Duration elapsed);

// skeleton_hash 改變時，把 digest 中所有有座標的持久物件遷到新骨架；
// 找不到合法位置的物件會進毀壞紀錄，且每次實際遷移必產生敘事事件。
[[nodiscard]] SiteMigrationReport migrate_site_digest(
    SiteDigest& digest, const SiteSkeleton& new_skeleton, const rules::Ruleset& ruleset);

// 把 digest 疊回已重建的 Site，並依 now-unload_tick 一次完成補算。
[[nodiscard]] SiteCatchUpReport restore_site_digest(zone::Zone& site,
                                                    const SiteFastVars& fast, time::Tick now,
                                                    const rules::Ruleset& ruleset);

}  // namespace aetheria::site
