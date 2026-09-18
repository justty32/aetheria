// dungeon.cpp：地城來源、歷史身分與深度選擇。

#include "core/local/dungeon.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "core/rules/power.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::local {
namespace {

constexpr std::uint64_t kHistoryEventSalt = UINT64_C(0x4849535444554E31);

[[nodiscard]] std::uint64_t historical_event_id(std::uint32_t region_id,
                                                std::uint32_t canonical_id,
                                                bool survived) noexcept {
    const auto identity = (static_cast<std::uint64_t>(region_id) << 32U) | canonical_id;
    return worldgen::splitmix64(kHistoryEventSalt ^ identity ^
                                (survived ? UINT64_C(1) << 63U : UINT64_C(0)));
}

[[nodiscard]] const worldgen::CitySite* ancient_site(
    std::uint32_t canonical_id, const worldgen::HistoryStageOutput& history) noexcept {
    const auto found = std::ranges::find(history.ancient_sites.cities, canonical_id,
                                         &worldgen::CitySite::canonical_id);
    return found == history.ancient_sites.cities.end() ? nullptr : &*found;
}

}  // namespace

std::uint8_t choose_dungeon_depth(DungeonDepthSource source, std::uint64_t seed,
                                  std::uint8_t story_depth) {
    switch (source) {
        case DungeonDepthSource::BeastLair:
            return static_cast<std::uint8_t>(1U + seed % 2U);
        case DungeonDepthSource::Mine:
            return static_cast<std::uint8_t>(3U + seed % 4U);
        case DungeonDepthSource::Ruin:
            return static_cast<std::uint8_t>(5U + seed % 6U);
        case DungeonDepthSource::Story:
            if (story_depth == 0 || story_depth > 64U) {
                throw std::invalid_argument{"劇情地城深度必須介於 1 與 64"};
            }
            return story_depth;
    }
    throw std::invalid_argument{"未知地城深度來源"};
}

std::optional<DungeonHistoryOrigin> dungeon_history_origin(
    std::uint32_t region_id, std::uint32_t ancient_site_canonical_id,
    const worldgen::HistoryStageOutput& history) noexcept {
    const auto* site = ancient_site(ancient_site_canonical_id, history);
    if (site == nullptr || ancient_site_canonical_id >= history.survivor.size()) {
        return std::nullopt;
    }
    const bool survived = history.survivor[ancient_site_canonical_id] != 0;
    return DungeonHistoryOrigin{
        historical_event_id(region_id, ancient_site_canonical_id, survived), region_id,
        ancient_site_canonical_id, site->tier, survived};
}

bool origin_matches_history(const DungeonHistoryOrigin& origin,
                            const worldgen::HistoryStageOutput& history) noexcept {
    const auto expected = dungeon_history_origin(origin.region_id,
                                                 origin.ancient_site_canonical_id, history);
    return expected.has_value() && *expected == origin;
}

}  // namespace aetheria::local
