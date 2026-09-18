// playable_session_site_view.cpp：Site 批次快照。

#include "core/runtime/playable_session.h"

#include "core/site/site_build_loop.h"


#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>


#include "core/runtime/playable_session_detail.h"

namespace aetheria::runtime {
using namespace detail;

std::optional<PlayableGridView> PlayableSession::site_view() const {
    const auto handle = manager_->get(coverage_site_key_);
    if (!handle.has_value()) {
        return std::nullopt;
    }
    PlayableGridView result{site::kSiteWidth, site::kSiteHeight, 0,
                            {}, 0, 0};
    const bool borrowed = manager_->with(*handle, [&](const zone::Zone& loaded) {
        const auto& layers = std::get<zone::SitePayload>(loaded.payload).layers;
        const auto tile_count = layers.procedural.skeleton.ground.size();
        result.cells.assign(tile_count, UINT8_C(0));
        if (tile_count != site::kSiteTileCount) {
            return;
        }
        for (std::size_t index = 0; index < result.cells.size(); ++index) {
            result.cells[index] = layers.procedural.skeleton.roads[index] != 0
                                      ? UINT8_C(1)
                                      : (layers.procedural.skeleton.buildable[index] != 0
                                             ? UINT8_C(2)
                                             : UINT8_C(0));
        }
        for (const auto& building : layers.procedural.buildings) {
            for (std::uint16_t y = building.origin.y;
                 y < static_cast<std::uint32_t>(building.origin.y) + building.height; ++y) {
                for (std::uint16_t x = building.origin.x;
                     x < static_cast<std::uint32_t>(building.origin.x) + building.width; ++x) {
                    result.cells[site_index({x, y})] = UINT8_C(3);
                }
            }
        }
        const auto states = loaded.reg.view<const site::CityBuildState>();
        if (!states.empty()) {
            const auto& state =
                states.get<const site::CityBuildState>(*states.begin());
            for (const auto& building : state.buildings) {
                const auto id = ruleset_.find_city_building(building.definition_id);
                const auto* definition = id.has_value() ? ruleset_.city_building(*id) : nullptr;
                if (definition == nullptr) {
                    continue;
                }
                for (std::uint16_t y = building.origin.y;
                     y < static_cast<std::uint32_t>(building.origin.y) + definition->height;
                     ++y) {
                    for (std::uint16_t x = building.origin.x;
                         x < static_cast<std::uint32_t>(building.origin.x) + definition->width;
                         ++x) {
                        result.cells[site_index({x, y})] = UINT8_C(4);
                    }
                }
            }
        }
        result.cells[site_index(local_entrance_)] = UINT8_C(5);
    });
    return borrowed ? std::optional<PlayableGridView>{std::move(result)}
                    : std::nullopt;
}

} // namespace aetheria::runtime
