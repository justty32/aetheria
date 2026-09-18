// playable_session_local.cpp：Local 進退與開門。

#include "core/runtime/playable_session.h"

#include "core/local/local_movement.h"
#include "core/local/local_reduction.h"

#include <aetheria/runtime/cross_zone.h>

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

namespace detail {

[[nodiscard]] local::DoorStateQuery door_query(const zone::Zone& local_zone) {
    const auto states = local_zone.reg.view<const local::LocalDoorState>();
    if (states.size() > 1U) {
        throw std::logic_error{"Local zone 含多個 LocalDoorState"};
    }
    const local::LocalDoorState* state =
        states.empty() ? nullptr
                       : &states.get<const local::LocalDoorState>(*states.begin());
    return [state](const local::LocalEdgeAddress& edge) {
        return state != nullptr && state->opened.contains(edge)
                   ? local::DoorState::Open
                   : local::DoorState::Closed;
    };
}

} // namespace detail

void PlayableSession::enter_local() {
    record_command("enter_local_manual");
    if (residence_ != PlayableResidence::Site) {
        throw std::logic_error{"只有 Site 駐留可親自進入 Local"};
    }
    static_cast<void>(acquire_coverage_local());
    residence_ = PlayableResidence::Local;
    local_z_ = 0;
    ++revision_;
    append_event(PlayableEventKind::LocalEntered, coverage_tile_, local_entrance_.x,
                 local_entrance_.y);
}

void PlayableSession::leave_local() {
    record_command("leave_local");
    if (residence_ != PlayableResidence::Local) {
        throw std::logic_error{"只有 Local 地表可返回 Site"};
    }
    const auto site_handle = manager_->get(coverage_site_key_);
    const auto local_handle = manager_->get(coverage_local_key_);
    if (!site_handle.has_value() || !local_handle.has_value()) {
        throw std::logic_error{"返回 Site 時父子 zone 未同時載入"};
    }
    const std::array handles{*site_handle, *local_handle};
    const bool borrowed = manager_->with_many(
        handles, [&](std::span<zone::Zone* const> zones) {
            auto& site_layers =
                std::get<zone::SitePayload>(zones[0]->payload).layers;
            static_cast<void>(local::reduce_live_local(site_layers, local_entrance_,
                                                       *zones[1]));
        });
    if (!borrowed || !manager_->unload(coverage_local_key_)) {
        throw std::logic_error{"Local 歸約或卸載失敗"};
    }
    residence_ = PlayableResidence::Site;
    local_z_ = 0;
    ++revision_;
    append_event(PlayableEventKind::LocalLeft, coverage_tile_);
}

void PlayableSession::open_door_and_move() {
    record_command("open_door");
    if (residence_ != PlayableResidence::Local || local_z_ != 0) {
        throw std::logic_error{"開門示範只接受尚未開門的 Local 地表"};
    }
    runtime::CrossZoneRuntime runtime{*manager_};
    const local::LocalLocation position{
        coverage_local_key_, {local_player_x_, local_player_y_}};
    const auto edge = local::local_edge_address(position, spatial::BoundarySide::East);
    const auto handle = manager_->get(coverage_local_key_);
    if (!edge.has_value() || !handle.has_value()) {
        throw std::logic_error{"開門示範找不到 Local 門 edge 或 zone"};
    }
    const bool borrowed = manager_->with(*handle, [&](zone::Zone& loaded) {
        const auto closed = local::assess_exploration_step(
            runtime, ruleset_, position, spatial::BoundarySide::East,
            door_query(loaded));
        if (closed != local::ExplorationStepResult::MustOpenDoor) {
            throw std::logic_error{"Local 門不是可開啟的關閉狀態"};
        }
        auto states = loaded.reg.view<local::LocalDoorState>();
        if (states.size() > 1U) {
            throw std::logic_error{"Local zone 含多個 LocalDoorState"};
        }
        local::LocalDoorState* state{};
        if (states.empty()) {
            const auto placeholders = loaded.reg.view<zone::ZoneMeta>();
            if (placeholders.size() != 1U) {
                throw std::logic_error{"Local zone 缺少唯一 ZoneMeta"};
            }
            state = &loaded.reg.emplace<local::LocalDoorState>(*placeholders.begin());
        } else {
            state = &states.get<local::LocalDoorState>(*states.begin());
        }
        state->opened.insert(*edge);
        const auto opened = local::assess_exploration_step(
            runtime, ruleset_, position, spatial::BoundarySide::East,
            door_query(loaded));
        if (opened != local::ExplorationStepResult::Allowed) {
            state->opened.erase(*edge);
            throw std::logic_error{"Local 開門後仍不能移動"};
        }
    });
    if (!borrowed) {
        throw std::logic_error{"開門示範借不到 Local zone"};
    }
    ++local_player_x_;
    ++revision_;
    append_event(PlayableEventKind::DoorOpened, coverage_tile_, local_player_x_,
                 local_player_y_);
}

} // namespace aetheria::runtime
