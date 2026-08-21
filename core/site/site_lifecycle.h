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

    template <typename Archive> void serialize(Archive& archive) {
        auto raw_tick = static_cast<std::int64_t>(unload_tick);
        archive(raw_tick, site_seed, skeleton_hash, objects, city_buildings, pending, economy,
                story_flags);
        unload_tick = time::Tick{raw_tick};
    }
    bool operator==(const SiteDigest&) const = default;
};

struct SiteCatchUpReport {
    std::uint64_t elapsed_seconds{};
    std::uint64_t aging_seconds_applied{};
    std::uint32_t pending_advanced{};
    std::uint32_t constructions_completed{};
    std::uint32_t persistent_objects_advanced{};
    std::uint32_t aging_transitions{};
    bool aging_cap_hit{};
};

[[nodiscard]] bool valid_site_digest(const SiteDigest& digest,
                                     const rules::Ruleset& ruleset) noexcept;

// 對 live 持久物件推進 elapsed；L_FULL 可小步呼叫，重載可一次閉式呼叫。
[[nodiscard]] SiteCatchUpReport advance_persistent_objects(
    SitePersistentLayer& persistent, const SiteFastVars& fast, time::Duration elapsed);

// 把 digest 疊回已重建的 Site，並依 now-unload_tick 一次完成補算。
[[nodiscard]] SiteCatchUpReport restore_site_digest(zone::Zone& site,
                                                    const SiteFastVars& fast, time::Tick now,
                                                    const rules::Ruleset& ruleset);

}  // namespace aetheria::site
