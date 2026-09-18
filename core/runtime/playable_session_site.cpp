// playable_session_site.cpp：Site 進退與取得。

#include "core/runtime/playable_session.h"

#include "core/site/site_materialize.h"


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


namespace aetheria::runtime {

zone::ZoneHandle PlayableSession::acquire_coverage_site() {
    const auto acquired = manager_->acquire(coverage_site_key_);
    if (!acquired.has_value()) {
        throw std::runtime_error{"ZoneManager::acquire 未能重展開覆蓋 Site"};
    }
    return *acquired;
}

zone::ZoneHandle PlayableSession::acquire_coverage_local() {
    static_cast<void>(acquire_coverage_site());
    const auto acquired = manager_->acquire(coverage_local_key_);
    if (!acquired.has_value()) {
        throw std::runtime_error{"ZoneManager::acquire 未能重展開覆蓋 Local"};
    }
    return *acquired;
}

void PlayableSession::enter_site() {
    record_command("enter_site_manual");
    perform_enter_site();
}

void PlayableSession::perform_enter_site() {
    if (residence_ != PlayableResidence::Region) {
        throw std::logic_error{"只有 Region 駐留可親自進入 Site"};
    }
    const auto handle = acquire_coverage_site();
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    const bool borrowed = manager_->with(handle, [&](zone::Zone& loaded) {
        site::enter_full_site(loaded, region_tiles, coverage_tile_);
    });
    if (!borrowed) {
        throw std::logic_error{"進入 Site 時 zone 已不在 manager"};
    }
    residence_ = PlayableResidence::Site;
    ++revision_;
    append_event(PlayableEventKind::SiteEntered, coverage_tile_);
}

void PlayableSession::leave_site() {
    record_command("leave_site");
    perform_leave_site();
}

void PlayableSession::perform_leave_site() {
    if (residence_ != PlayableResidence::Site) {
        throw std::logic_error{"只有 Site 駐留可返回 Region"};
    }
    const auto handle = manager_->get(coverage_site_key_);
    if (!handle.has_value()) {
        throw std::logic_error{"返回 Region 時 Site 未載入"};
    }
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    const bool borrowed = manager_->with(*handle, [&](zone::Zone& loaded) {
        refresh_quests(loaded);
    });
    if (!borrowed) {
        throw std::logic_error{"返回 Region 前無法讀取 Site"};
    }
    site::unload_site_zone(*manager_, *handle, region_tiles, coverage_tile_, seed_,
                           region_id_, now(), ruleset_);
    residence_ = PlayableResidence::Region;
    ++revision_;
    append_event(PlayableEventKind::SiteLeft, coverage_tile_);
}

} // namespace aetheria::runtime
