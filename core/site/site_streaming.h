#pragma once

// site_streaming.h：以 observer 場強協調多個 Site 的 LOD、延遲逐出與批次旬結算。

#include "core/site/site_build_loop.h"
#include "core/site/site_materialize.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

namespace aetheria::site {

inline constexpr std::int16_t kStreamingFullRadius = 1;
inline constexpr std::int16_t kStreamingCoarseRadius = 2;
inline constexpr time::Duration kStreamingAbsentDelay = time::kXun;

struct StreamingLodCounts {
    std::size_t full{};
    std::size_t coarse{};
    std::size_t frozen{};

    constexpr bool operator==(const StreamingLodCounts&) const noexcept = default;
};

struct StreamingTransitionReport {
    StreamingLodCounts lod;
    std::uint64_t loads{};
    std::uint64_t unloads{};
    std::uint64_t promotions{};
    std::uint64_t demotions{};
};

// SiteStreamingCoordinator 借用單一 Region、ZoneManager 與 store。
// player_crossed_tile 只重算暫態場；finish_turn 才做結構性升降級與逐出。
class SiteStreamingCoordinator {
public:
    SiteStreamingCoordinator(const rules::Ruleset& ruleset, zone::ZoneStore& store,
                             zone::ZoneManager& manager, zone::Zone& region,
                             std::int8_t region_z, std::uint64_t world_seed);

    void player_crossed_tile(world::RegionXY coordinate);
    [[nodiscard]] SiteBatchAdvanceReport advance_hours(std::uint32_t hours);
    [[nodiscard]] StreamingTransitionReport finish_turn();

    [[nodiscard]] StreamingLodCounts lod_counts() const;
    [[nodiscard]] std::uint64_t load_count() const noexcept { return load_count_; }
    [[nodiscard]] std::uint64_t unload_count() const noexcept { return unload_count_; }
    [[nodiscard]] std::uint64_t field_recompute_count() const noexcept {
        return field_recompute_count_;
    }

private:
    struct LoadedSite {
        zone::ZoneHandle handle;
        world::RegionXY coordinate;
        std::optional<time::Tick> frozen_since;
    };

    [[nodiscard]] world::RegionTiles& region_tiles();
    [[nodiscard]] const world::RegionTiles& region_tiles() const;
    [[nodiscard]] time::Tick now() const;
    [[nodiscard]] LoadedSite& ensure_loaded(world::RegionXY coordinate,
                                            StreamingTransitionReport& report);

    const rules::Ruleset& ruleset_;
    zone::ZoneStore& store_;
    zone::ZoneManager& manager_;
    zone::Zone& region_;
    std::int8_t region_z_{};
    std::uint64_t world_seed_{};
    std::uint32_t region_id_{};
    SiteTurnPipeline site_turn_;
    std::map<zone::ZoneKey, zone::LodLevel> desired_;
    std::map<zone::ZoneKey, LoadedSite> loaded_;
    std::optional<world::RegionXY> player_;
    std::uint64_t load_count_{};
    std::uint64_t unload_count_{};
    std::uint64_t field_recompute_count_{};
};

}  // namespace aetheria::site
