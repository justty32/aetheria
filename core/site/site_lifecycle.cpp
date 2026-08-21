// site_lifecycle.cpp：SiteDigest 驗證、持久物件老化與未完成建造閉式補算。

#include "core/site/site_lifecycle.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace aetheria::site {
namespace {

[[nodiscard]] std::uint8_t state_rank(BuildingState state) noexcept {
    return static_cast<std::uint8_t>(state);
}

[[nodiscard]] BuildingState state_for_rank(std::uint8_t rank) noexcept {
    return static_cast<BuildingState>(std::min<std::uint8_t>(rank, 3));
}

[[nodiscard]] BuildingState environment_state(const SiteFastVars& fast) noexcept {
    if (fast.damage >= 90U) {
        return BuildingState::Ruined;
    }
    if (fast.damage >= 60U) {
        return BuildingState::Derelict;
    }
    if (fast.owner == world::FactionId{}) {
        return BuildingState::Idle;
    }
    return BuildingState::Active;
}

}  // namespace

bool valid_site_digest(const SiteDigest& digest, const rules::Ruleset& ruleset) noexcept {
    if (!time::is_representable(digest.unload_tick)) {
        return false;
    }
    SitePersistentLayer persistent{digest.objects};
    CityBuildState state{digest.city_buildings, digest.pending, digest.economy};
    return valid_persistent_layer(persistent) && valid_city_build_state(state, ruleset);
}

SiteCatchUpReport advance_persistent_objects(SitePersistentLayer& persistent,
                                             const SiteFastVars& fast,
                                             time::Duration elapsed) {
    const auto raw_elapsed = static_cast<std::int64_t>(elapsed);
    if (raw_elapsed < 0) {
        throw std::invalid_argument{"持久層補算不能倒退時間"};
    }
    SiteCatchUpReport report;
    report.elapsed_seconds = static_cast<std::uint64_t>(raw_elapsed);
    const auto cap = static_cast<std::int64_t>(kBuildingAgingCap);
    const auto applied = std::min(raw_elapsed, cap);
    report.aging_seconds_applied = static_cast<std::uint64_t>(applied);
    report.aging_cap_hit = raw_elapsed > cap;
    const auto environment = environment_state(fast);
    for (auto& building : persistent.buildings) {
        const auto before = state_rank(building.state);
        const bool adverse = fast.owner == world::FactionId{} || fast.damage >= 50U;
        if (adverse && applied > 0) {
            const auto old_age = static_cast<std::int64_t>(building.aging_seconds);
            const auto next_age = std::min(cap, old_age + applied);
            building.aging_seconds = static_cast<std::uint32_t>(next_age);
            ++report.persistent_objects_advanced;
        }
        const auto age_rank = static_cast<std::uint8_t>(
            std::min<std::int64_t>(3, building.aging_seconds /
                                         static_cast<std::int64_t>(kBuildingAgingStep)));
        const auto next = std::max(state_rank(environment), age_rank);
        building.state = state_for_rank(next);
        if (next > before) {
            report.aging_transitions += static_cast<std::uint32_t>(next - before);
        }
    }
    return report;
}

SiteCatchUpReport restore_site_digest(zone::Zone& site, const SiteFastVars& fast,
                                      time::Tick now, const rules::Ruleset& ruleset) {
    auto digests = site.reg.view<SiteDigest>();
    if (digests.size() != 1U) {
        throw std::logic_error{"Site 重載補算要求恰有一份 SiteDigest"};
    }
    const auto entity = *digests.begin();
    auto digest = digests.get<SiteDigest>(entity);
    if (!valid_site_digest(digest, ruleset)) {
        throw std::runtime_error{"SiteDigest 內容無效"};
    }
    const auto elapsed = now - digest.unload_tick;
    const auto raw_elapsed = static_cast<std::int64_t>(elapsed);
    if (raw_elapsed < 0) {
        throw std::invalid_argument{"Site 重載時鐘早於卸載時鐘"};
    }

    auto& persistent = std::get<zone::SitePayload>(site.payload).layers.persistent;
    persistent.buildings = digest.objects;
    auto report = advance_persistent_objects(persistent, fast, elapsed);

    CityBuildState state{std::move(digest.city_buildings), std::move(digest.pending),
                         digest.economy};
    state.economy.population = fast.population;
    state.economy.food_stock = fast.food_stock;
    state.economy.production_stock = fast.production_stock;
    const auto elapsed_hours = static_cast<std::uint64_t>(raw_elapsed) /
                               static_cast<std::uint64_t>(time::kHour);
    for (auto& construction : state.pending) {
        ++report.pending_advanced;
        if (elapsed_hours >= construction.remaining_hours) {
            construction.remaining_hours = 0;
        } else {
            construction.remaining_hours = static_cast<std::uint16_t>(
                construction.remaining_hours - elapsed_hours);
        }
    }
    for (auto iterator = state.pending.begin(); iterator != state.pending.end();) {
        if (iterator->remaining_hours != 0) {
            ++iterator;
            continue;
        }
        state.buildings.push_back({std::move(iterator->definition_id), iterator->origin});
        iterator = state.pending.erase(iterator);
        ++report.constructions_completed;
    }
    state.economy.hours_into_xun = static_cast<std::uint16_t>(
        (state.economy.hours_into_xun + elapsed_hours) % 240U);
    site.reg.emplace_or_replace<CityBuildState>(entity, std::move(state));
    site.reg.remove<SiteDigest>(entity);
    return report;
}

}  // namespace aetheria::site
