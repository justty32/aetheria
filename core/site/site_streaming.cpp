// site_streaming.cpp：玩家跨格重算場強，回合尾端統一套用 Site LOD 結構變更。

#include "core/site/site_streaming.h"

#include "core/observer/field.h"
#include "core/world/region_movement.h"
#include "core/zone/zone_key.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aetheria::site {

SiteStreamingCoordinator::SiteStreamingCoordinator(
    const rules::Ruleset& ruleset, zone::ZoneStore& store, zone::ZoneManager& manager,
    zone::Zone& region, std::int8_t region_z, std::uint64_t world_seed)
    : ruleset_{ruleset}, store_{store}, manager_{manager}, region_{region},
      region_z_{region_z}, world_seed_{world_seed}, region_id_{zone::region_id_of(region.key)},
      site_turn_{ruleset, store} {
    if (zone::level_of(region_.key) != zone::ZoneLevel::Region) {
        throw std::invalid_argument{"Site streaming 只接受 Region zone"};
    }
    static_cast<void>(region_tiles());
    static_cast<void>(world::turn_clock(region_));
}

world::RegionTiles& SiteStreamingCoordinator::region_tiles() {
    auto* payload = std::get_if<zone::RegionPayload>(&region_.payload);
    if (payload == nullptr || !payload->layers.contains(region_z_)) {
        throw std::invalid_argument{"Site streaming 找不到 Region layer"};
    }
    return payload->layers.at(region_z_);
}

const world::RegionTiles& SiteStreamingCoordinator::region_tiles() const {
    const auto* payload = std::get_if<zone::RegionPayload>(&region_.payload);
    if (payload == nullptr || !payload->layers.contains(region_z_)) {
        throw std::invalid_argument{"Site streaming 找不到 Region layer"};
    }
    return payload->layers.at(region_z_);
}

time::Tick SiteStreamingCoordinator::now() const { return world::turn_clock(region_).now; }

void SiteStreamingCoordinator::player_crossed_tile(world::RegionXY coordinate) {
    const auto& tiles = region_tiles();
    static_cast<void>(tiles.index_of(coordinate));
    player_ = coordinate;
    desired_.clear();
    for (std::int16_t dy = -kStreamingCoarseRadius; dy <= kStreamingCoarseRadius; ++dy) {
        for (std::int16_t dx = -kStreamingCoarseRadius; dx <= kStreamingCoarseRadius; ++dx) {
            const auto x = static_cast<std::int32_t>(coordinate.x) + dx;
            const auto y = static_cast<std::int32_t>(coordinate.y) + dy;
            if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(tiles.width) ||
                y >= static_cast<std::int32_t>(tiles.height)) {
                continue;
            }
            const auto distance = std::max(std::abs(dx), std::abs(dy));
            const auto score = observer::field_score(kStreamingCoarseRadius, distance);
            desired_.emplace(zone::child_key(region_.key, static_cast<std::uint32_t>(x),
                                             static_cast<std::uint32_t>(y)),
                             score >= kStreamingCoarseRadius - kStreamingFullRadius
                                 ? zone::LodLevel::Full
                                 : zone::LodLevel::Coarse);
        }
    }
    ++field_recompute_count_;
}

SiteStreamingCoordinator::LoadedSite& SiteStreamingCoordinator::ensure_loaded(
    world::RegionXY coordinate, StreamingTransitionReport& report) {
    const auto key = zone::child_key(region_.key, static_cast<std::uint32_t>(coordinate.x),
                                     static_cast<std::uint32_t>(coordinate.y));
    if (const auto found = loaded_.find(key); found != loaded_.end()) {
        return found->second;
    }

    const auto acquired = manager_.acquire(
        key, [&](zone::ZoneKey requested, std::unique_ptr<zone::Zone> persistent) {
            if (requested != key) {
                return std::unique_ptr<zone::Zone>{};
            }
            if (persistent != nullptr) {
                auto site = rematerialize_site_zone(
                    std::move(*persistent), region_tiles(), coordinate, world_seed_, region_id_,
                    now(), ruleset_);
                return std::make_unique<zone::Zone>(std::move(site));
            }
            auto site = materialize_site_zone(region_tiles(), coordinate, world_seed_, region_id_,
                                              ruleset_);
            return std::make_unique<zone::Zone>(std::move(site));
        });
    if (!acquired.has_value()) {
        throw std::runtime_error{"Site streaming 無法取得完整 Site zone"};
    }
    const auto handle = *acquired;
    auto [position, inserted] = loaded_.emplace(key, LoadedSite{handle, coordinate, std::nullopt});
    if (!inserted) {
        throw std::logic_error{"Site streaming 重複登記 loaded zone"};
    }
    ++load_count_;
    ++report.loads;
    return position->second;
}

SiteBatchAdvanceReport SiteStreamingCoordinator::advance_hours(std::uint32_t hours) {
    std::vector<zone::ZoneHandle> handles;
    std::vector<world::RegionXY> coordinates;
    for (const auto& [key, loaded] : loaded_) {
        static_cast<void>(key);
        const auto index = region_tiles().index_of(loaded.coordinate);
        if (region_tiles().site.at(index).lod == zone::LodLevel::Full) {
            handles.push_back(loaded.handle);
            coordinates.push_back(loaded.coordinate);
        }
    }
    if (handles.empty()) {
        if (hours != 0) {
            throw std::logic_error{"Site streaming 尚無 L_FULL zone 可推進"};
        }
        return {};
    }

    SiteBatchAdvanceReport report;
    const bool borrowed = manager_.with_many(handles, [&](std::span<zone::Zone* const> zones) {
        std::vector<SiteAdvanceTarget> targets;
        targets.reserve(zones.size());
        for (std::size_t index = 0; index < zones.size(); ++index) {
            targets.push_back({zones[index], region_z_, coordinates[index]});
        }
        report = site_turn_.advance_hours(region_, targets, hours);
    });
    if (!borrowed) {
        throw std::logic_error{"Site streaming 的 loaded handle 在批次推進前失效"};
    }
    return report;
}

}  // namespace aetheria::site
