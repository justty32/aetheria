// playable_session_city.cpp：城建操作。

#include "core/runtime/playable_session.h"

#include "core/site/site_build_loop.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"


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

void PlayableSession::perform_city_build(bool managed) {
    if ((!managed && residence_ != PlayableResidence::Site) ||
        (managed && residence_ != PlayableResidence::Region)) {
        throw std::logic_error{"城建命令的駐留層不符"};
    }
    const auto handle = acquire_coverage_site();
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    site::SiteAdvanceReport report;
    const bool borrowed = manager_->with(handle, [&](zone::Zone& loaded) {
        site::enter_full_site(loaded, region_tiles, coverage_tile_);
        auto& state = site::city_build_state(loaded);
        if (std::ranges::any_of(state.buildings, [](const auto& building) {
                return building.definition_id == "city.house";
            }) ||
            std::ranges::any_of(state.pending, [](const auto& building) {
                return building.definition_id == "city.house";
            })) {
            throw std::logic_error{"示範住宅已經蓋過"};
        }
        const auto id = ruleset_.find_city_building("city.house");
        const auto* definition = id.has_value() ? ruleset_.city_building(*id) : nullptr;
        if (definition == nullptr) {
            throw std::runtime_error{"城建示範缺少 city.house"};
        }
        const auto& procedural =
            std::get<zone::SitePayload>(loaded.payload).layers.procedural;
        std::optional<site::SiteXY> origin;
        for (std::uint16_t y = 0; y + definition->height <= site::kSiteHeight &&
                                  !origin.has_value();
             ++y) {
            for (std::uint16_t x = 0; x + definition->width <= site::kSiteWidth;
                 ++x) {
                bool legal = true;
                for (std::uint16_t dy = 0; dy < definition->height && legal; ++dy) {
                    for (std::uint16_t dx = 0; dx < definition->width; ++dx) {
                        const auto index = site_index({static_cast<std::uint16_t>(x + dx),
                                                       static_cast<std::uint16_t>(y + dy)});
                        if (procedural.skeleton.buildable[index] == 0 ||
                            procedural.skeleton.roads[index] != 0) {
                            legal = false;
                            break;
                        }
                    }
                }
                if (legal) {
                    origin = site::SiteXY{x, y};
                    break;
                }
            }
        }
        if (!origin.has_value()) {
            throw std::runtime_error{"城區找不到 2x2 可建位置"};
        }
        last_development_before_ =
            region_tiles.reduction_value<world::DevelopmentLevelReduction>(coverage_tile_);
        site::start_construction(loaded, "city.house", *origin, ruleset_);
        site::SiteTurnPipeline pipeline{ruleset_, store_};
        report = pipeline.advance_hours(loaded, *region_, 0, coverage_tile_,
                                        definition->construction_hours);
        if (report.constructions_completed != 1U) {
            throw std::logic_error{"住宅施工沒有完成恰好一棟"};
        }
        site::reduce_live_site_xun(region_tiles, coverage_tile_, loaded, ruleset_);
        last_development_after_ =
            region_tiles.reduction_value<world::DevelopmentLevelReduction>(coverage_tile_);
        refresh_quests(loaded);
    });
    if (!borrowed) {
        throw std::logic_error{"城建時 Site 未載入"};
    }
    if (managed) {
        site::unload_site_zone(*manager_, handle, region_tiles, coverage_tile_, seed_,
                               region_id_, now(), ruleset_);
        append_event(PlayableEventKind::ManagedActivity, coverage_tile_,
                     last_development_before_, last_development_after_);
    }
    ++revision_;
    append_event(PlayableEventKind::ConstructionCompleted, coverage_tile_,
                 last_development_before_, last_development_after_);
}

void PlayableSession::build_city() {
    record_command("build_city");
    perform_city_build(false);
}

void PlayableSession::manage_city() {
    record_command("enter_site_auto");
    perform_city_build(true);
}

} // namespace aetheria::runtime
