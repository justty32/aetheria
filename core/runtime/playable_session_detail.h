#pragma once

#include "core/runtime/playable_session.h"
#include "core/local/local_movement.h"

namespace aetheria::runtime::detail {

[[nodiscard]] constexpr std::size_t site_index(site::SiteXY tile) noexcept {
    return static_cast<std::size_t>(tile.y) * site::kSiteWidth + tile.x;
}

[[nodiscard]] constexpr std::size_t local_index(local::LocalXY tile) noexcept {
    return static_cast<std::size_t>(tile.y) * local::kLocalWidth + tile.x;
}

void install_coverage_dungeon_entrance(site::SiteLayers& layers,
                                       site::SiteXY entrance,
                                       const rules::Ruleset& ruleset);
[[nodiscard]] local::DoorStateQuery door_query(const zone::Zone& local_zone);
[[nodiscard]] std::string_view residence_id(PlayableResidence residence);

} // namespace aetheria::runtime::detail
