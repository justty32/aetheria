// playable_session_coverage.cpp：覆蓋情境摘要。

#include "core/runtime/playable_session.h"

#include "core/site/site_build_loop.h"
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


namespace aetheria::runtime {

PlayableCoverageSummary PlayableSession::coverage_summary() const {
    const auto& region_tiles = tiles();
    PlayableCoverageSummary result;
    result.residence = residence_;
    result.development =
        region_tiles.reduction_value<world::DevelopmentLevelReduction>(coverage_tile_);
    result.order = region_tiles.reduction_value<world::OrderReduction>(coverage_tile_);
    result.production =
        region_tiles.reduction_value<world::ProductionStockReduction>(coverage_tile_);
    result.quest_count = static_cast<std::uint32_t>(quests_.size());
    result.bandit_quest_available =
        std::ranges::any_of(quests_, [](const auto& quest) {
            return quest.kind == narrative::EmergentQuestKind::BanditSuppression;
        });
    result.bandit_quest_accepted = accepted_quest_id_.has_value();
    result.dungeon_quest_available =
        std::ranges::any_of(quests_, [](const auto& quest) {
            return quest.kind == narrative::EmergentQuestKind::DungeonExploration;
        });
    result.dungeon_cleared = false;

    const auto inspect_site = [&](const zone::Zone& site_zone) {
        const auto states = site_zone.reg.view<const site::CityBuildState>();
        if (!states.empty()) {
            result.city_buildings = static_cast<std::uint32_t>(
                states.get<const site::CityBuildState>(*states.begin()).buildings.size());
        }
        const auto& persistent =
            std::get<zone::SitePayload>(site_zone.payload).layers.persistent;
        result.dungeon_cleared = !persistent.dungeons.empty() &&
                                 persistent.dungeons.front().cleared;
        result.persistent_buildings = static_cast<std::uint32_t>(persistent.buildings.size());
        result.persistent_population =
            *site::ReductionTable::reduce(std::get<zone::SitePayload>(site_zone.payload).layers)
                 .value<world::PopulationReduction>();
    };
    if (const auto loaded = manager_->get(coverage_site_key_)) {
        static_cast<void>(manager_->with(*loaded, inspect_site));
    } else if (const auto stored = store_.load(coverage_site_key_)) {
        inspect_site(*stored);
    }

    result.dungeon_density_before = dungeon_density_before_;
    result.dungeon_density_after = dungeon_density_after_;
    result.treaty_count = static_cast<std::uint32_t>(diplomacy_->treaties().size());
    result.last_order_before = last_order_before_;
    result.last_order_after = last_order_after_;
    result.last_development_before = last_development_before_;
    result.last_development_after = last_development_after_;
    result.roundtrip_hashes = roundtrip_hashes_;
    result.calibration_n = calibration_n_;
    result.manual_total = calibration_manual_total_;
    result.managed_total = calibration_managed_total_;
    result.signed_relative_error_percent = calibration_signed_error_percent_;
    return result;
}

} // namespace aetheria::runtime
