// playable_session_materialize.cpp：zone 具現化。

#include "core/runtime/playable_session.h"

#include "core/local/local_materialize.h"
#include "core/site/site_materialize.h"
#include "core/worldgen/region_seed.h"


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

std::unique_ptr<zone::Zone> PlayableSession::materialize_session_zone(
    zone::ZoneKey key, std::unique_ptr<zone::Zone> persistent) {
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    if (key == battle_site_key_) {
        if (persistent == nullptr) {
            return std::make_unique<zone::Zone>(site::materialize_site_zone(
                region_tiles, battle_tile_, seed_, region_id_, ruleset_));
        }
        return std::make_unique<zone::Zone>(site::rematerialize_site_zone(
            std::move(*persistent), region_tiles, battle_tile_, seed_, region_id_,
            now(), ruleset_));
    }
    if (key == coverage_site_key_) {
        if (persistent == nullptr) {
            auto result = std::make_unique<zone::Zone>(site::materialize_site_zone(
                region_tiles, coverage_tile_, seed_, region_id_, ruleset_));
            install_coverage_dungeon_entrance(
                std::get<zone::SitePayload>(result->payload).layers,
                local_entrance_, ruleset_);
            return result;
        }
        auto result = std::make_unique<zone::Zone>(site::rematerialize_site_zone(
            std::move(*persistent), region_tiles, coverage_tile_, seed_, region_id_,
            now(), ruleset_));
        install_coverage_dungeon_entrance(
            std::get<zone::SitePayload>(result->payload).layers,
            local_entrance_, ruleset_);
        return result;
    }
    if (key != coverage_local_key_) {
        return nullptr;
    }

    std::optional<zone::Zone> generated;
    const auto site_handle = manager_->get(coverage_site_key_);
    if (!site_handle.has_value()) {
        throw std::logic_error{"具現化 Local 前必須先載入父 Site"};
    }
    const bool borrowed = manager_->with(*site_handle, [&](const zone::Zone& parent) {
        const auto& parent_layer =
            std::get<zone::SitePayload>(parent.payload).layers.procedural;
        const auto feature = region_tiles.feature[region_tiles.index_of(coverage_tile_)];
        generated.emplace(local::materialize_local_zone(
            coverage_site_key_, parent_layer, local_entrance_,
            site::derive_site_seed(seed_, region_id_, coverage_tile_.x,
                                   coverage_tile_.y),
            feature, ruleset_));
    });
    if (!borrowed || !generated.has_value()) {
        throw std::logic_error{"父 Site 未能產生 Local"};
    }

    auto& generated_payload = std::get<zone::LocalPayload>(generated->payload);
    const auto door = ruleset_.find_edge("edge.house_door");
    if (!door.has_value()) {
        throw std::runtime_error{"可玩覆蓋情境缺少門 edge def"};
    }
    auto& ground = generated_payload.layers.at(0);
    const local::LocalXY from{31, 32};
    const local::LocalXY to{32, 32};
    ground.edges[local_index(from) * 4U +
                 static_cast<std::size_t>(spatial::BoundarySide::East)] = *door;
    ground.edges[local_index(to) * 4U +
                 static_cast<std::size_t>(spatial::BoundarySide::West)] = *door;

    if (persistent == nullptr) {
        return std::make_unique<zone::Zone>(std::move(*generated));
    }
    auto& persistent_payload = std::get<zone::LocalPayload>(persistent->payload);
    persistent_payload.layers = std::move(generated_payload.layers);
    persistent->lod = zone::LodLevel::Full;
    return persistent;
}

} // namespace aetheria::runtime
