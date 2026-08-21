// site_streaming.cpp：玩家跨格重算場強，回合尾端統一套用 Site LOD 結構變更。

#include "core/site/site_streaming.h"

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
            desired_.emplace(zone::child_key(region_.key, static_cast<std::uint32_t>(x),
                                             static_cast<std::uint32_t>(y)),
                             distance <= kStreamingFullRadius ? zone::LodLevel::Full
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

    zone::ZoneHandle handle = [&] {
        if (store_.contains(key)) {
            return rematerialize_site_zone(manager_, region_tiles(), coordinate, world_seed_,
                                            region_id_, now(), ruleset_);
        }
        auto site = materialize_site_zone(region_tiles(), coordinate, world_seed_, region_id_,
                                          ruleset_);
        return manager_.adopt(std::make_unique<zone::Zone>(std::move(site)));
    }();
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

StreamingTransitionReport SiteStreamingCoordinator::finish_turn() {
    if (!player_.has_value()) {
        throw std::logic_error{"Site streaming 尚未收到玩家位置"};
    }
    StreamingTransitionReport report;
    const auto current_time = now();

    for (const auto& [key, desired_lod] : desired_) {
        const world::RegionXY coordinate{static_cast<std::int16_t>(zone::site_x_of(key)),
                                         static_cast<std::int16_t>(zone::site_y_of(key))};
        auto& loaded = ensure_loaded(coordinate, report);
        auto& state = region_tiles().site.at(region_tiles().index_of(coordinate));
        if (state.lod == zone::LodLevel::Frozen) {
            thaw_site_zone(manager_, loaded.handle, region_tiles(), coordinate, world_seed_,
                           region_id_, current_time, ruleset_);
            loaded.frozen_since.reset();
            ++report.promotions;
        }
        if (state.lod == zone::LodLevel::Coarse && desired_lod == zone::LodLevel::Full) {
            const bool borrowed = manager_.with(loaded.handle, [&](zone::Zone& site) {
                enter_full_site(site, region_tiles(), coordinate);
            });
            if (!borrowed) {
                throw std::logic_error{"Site streaming 升到 L_FULL 時 handle 失效"};
            }
            ++report.promotions;
        } else if (state.lod == zone::LodLevel::Full &&
                   desired_lod == zone::LodLevel::Coarse) {
            const bool borrowed = manager_.with(loaded.handle, [&](zone::Zone& site) {
                site.lod = zone::LodLevel::Coarse;
            });
            if (!borrowed) {
                throw std::logic_error{"Site streaming 降到 L_COARSE 時 handle 失效"};
            }
            state.lod = zone::LodLevel::Coarse;
            ++report.demotions;
        } else if (state.lod == zone::LodLevel::Coarse) {
            // 即使只進入過粗載外圈，也先建立可凍結的持久城建狀態。
            const bool borrowed = manager_.with(loaded.handle, [&](zone::Zone& site) {
                const auto states = site.reg.view<CityBuildState>();
                if (states.empty()) {
                    enter_full_site(site, region_tiles(), coordinate);
                } else if (states.size() != 1U) {
                    throw std::logic_error{"Site streaming 的 L_COARSE 城建狀態不唯一"};
                }
                site.lod = zone::LodLevel::Coarse;
            });
            if (!borrowed) {
                throw std::logic_error{"Site streaming 初始化 L_COARSE 時 handle 失效"};
            }
            state.lod = zone::LodLevel::Coarse;
        }
    }

    const auto player_key = zone::child_key(
        region_.key, static_cast<std::uint32_t>(player_->x),
        static_cast<std::uint32_t>(player_->y));
    for (auto& [key, loaded] : loaded_) {
        const bool borrowed = manager_.with(loaded.handle, [&](zone::Zone& site) {
            site.pinned = key == player_key;
        });
        if (!borrowed) {
            throw std::logic_error{"Site streaming 更新 pinned 時 handle 失效"};
        }
    }

    for (auto& [key, loaded] : loaded_) {
        if (desired_.contains(key)) {
            continue;
        }
        auto& state = region_tiles().site.at(region_tiles().index_of(loaded.coordinate));
        if (state.lod == zone::LodLevel::Full || state.lod == zone::LodLevel::Coarse) {
            freeze_site_zone(manager_, loaded.handle, region_tiles(), loaded.coordinate,
                             world_seed_, region_id_, current_time);
            loaded.frozen_since = current_time;
            ++report.demotions;
        }
    }

    for (auto iterator = loaded_.begin(); iterator != loaded_.end();) {
        auto& loaded = iterator->second;
        if (!desired_.contains(iterator->first) && loaded.frozen_since.has_value() &&
            current_time - *loaded.frozen_since >= kStreamingAbsentDelay) {
            evict_frozen_site_zone(manager_, loaded.handle, region_tiles(), loaded.coordinate,
                                   current_time);
            ++unload_count_;
            ++report.unloads;
            ++report.demotions;
            iterator = loaded_.erase(iterator);
        } else {
            ++iterator;
        }
    }
    report.lod = lod_counts();
    return report;
}

StreamingLodCounts SiteStreamingCoordinator::lod_counts() const {
    StreamingLodCounts result;
    const auto& tiles = region_tiles();
    for (const auto& [key, loaded] : loaded_) {
        static_cast<void>(key);
        switch (tiles.site.at(tiles.index_of(loaded.coordinate)).lod) {
        case zone::LodLevel::Full:
            ++result.full;
            break;
        case zone::LodLevel::Coarse:
            ++result.coarse;
            break;
        case zone::LodLevel::Frozen:
            ++result.frozen;
            break;
        case zone::LodLevel::Absent:
            throw std::logic_error{"Site streaming loaded 表含 L_ABSENT"};
        }
    }
    return result;
}

}  // namespace aetheria::site
