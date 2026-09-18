// Site streaming 回合尾端 LOD 轉換、延遲逐出與層級統計。

#include "core/site/site_streaming.h"

#include "core/zone/zone_key.h"

#include <stdexcept>

namespace aetheria::site {

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
                             world_seed_, region_id_, current_time, ruleset_);
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
