// site_lifecycle.cpp：SiteDigest 驗證、持久物件老化與未完成建造閉式補算。

#include "core/site/site_lifecycle.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

struct Footprint {
    std::uint8_t width{1};
    std::uint8_t height{1};
};

[[nodiscard]] bool can_place(const SiteSkeleton& skeleton,
                             const std::array<std::uint8_t, kSiteTileCount>& occupied,
                             SiteXY origin, Footprint footprint) noexcept {
    const auto x_end = static_cast<std::uint32_t>(origin.x) + footprint.width;
    const auto y_end = static_cast<std::uint32_t>(origin.y) + footprint.height;
    if (!skeleton.valid_layout() || x_end > kSiteWidth || y_end > kSiteHeight) {
        return false;
    }
    for (std::uint32_t y = origin.y; y < y_end; ++y) {
        for (std::uint32_t x = origin.x; x < x_end; ++x) {
            const auto index = static_cast<std::size_t>(y) * kSiteWidth + x;
            if (skeleton.buildable[index] == 0 || occupied[index] != 0) {
                return false;
            }
        }
    }
    return true;
}

void occupy(std::array<std::uint8_t, kSiteTileCount>& occupied, SiteXY origin,
            Footprint footprint) noexcept {
    for (std::uint32_t y = origin.y;
         y < static_cast<std::uint32_t>(origin.y) + footprint.height; ++y) {
        for (std::uint32_t x = origin.x;
             x < static_cast<std::uint32_t>(origin.x) + footprint.width; ++x) {
            occupied[static_cast<std::size_t>(y) * kSiteWidth + x] = UINT8_C(1);
        }
    }
}

[[nodiscard]] std::optional<SiteXY> nearest_legal_origin(
    const SiteSkeleton& skeleton,
    const std::array<std::uint8_t, kSiteTileCount>& occupied, SiteXY former,
    Footprint footprint) noexcept {
    std::optional<SiteXY> best;
    std::uint32_t best_distance = std::numeric_limits<std::uint32_t>::max();
    for (std::uint16_t y = 0; y < kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < kSiteWidth; ++x) {
            const SiteXY candidate{x, y};
            if (!can_place(skeleton, occupied, candidate, footprint)) {
                continue;
            }
            const auto x_distance = former.x > x ? former.x - x : x - former.x;
            const auto y_distance = former.y > y ? former.y - y : y - former.y;
            const auto distance = static_cast<std::uint32_t>(x_distance) + y_distance;
            if (distance < best_distance) {
                best = candidate;
                best_distance = distance;
            }
        }
    }
    return best;
}

[[nodiscard]] Footprint city_footprint(std::string_view definition_id,
                                       const rules::Ruleset& ruleset) {
    const auto found = ruleset.find_city_building(definition_id);
    const auto* definition = found.has_value() ? ruleset.city_building(*found) : nullptr;
    if (definition == nullptr) {
        throw std::runtime_error{"骨架遷移引用不存在的 building def：" +
                                 std::string{definition_id}};
    }
    return {definition->width, definition->height};
}

template <typename Object, typename Origin, typename FootprintFor, typename DestroyedRecord>
void migrate_group(std::vector<Object>& objects, const SiteSkeleton& skeleton,
                   std::array<std::uint8_t, kSiteTileCount>& occupied, Origin origin_of,
                   FootprintFor footprint_for, DestroyedRecord destroyed_record,
                   SiteMigrationHistory& history, SiteMigrationReport& report) {
    std::vector<std::uint8_t> needs_migration(objects.size());
    for (std::size_t index = 0; index < objects.size(); ++index) {
        const auto footprint = footprint_for(objects[index]);
        const auto origin = origin_of(objects[index]);
        if (can_place(skeleton, occupied, origin, footprint)) {
            occupy(occupied, origin, footprint);
            ++report.retained;
        } else {
            needs_migration[index] = UINT8_C(1);
        }
    }

    std::vector<Object> survivors;
    survivors.reserve(objects.size());
    for (std::size_t index = 0; index < objects.size(); ++index) {
        auto& object = objects[index];
        if (needs_migration[index] == 0) {
            survivors.push_back(std::move(object));
            continue;
        }
        const auto footprint = footprint_for(object);
        const auto destination =
            nearest_legal_origin(skeleton, occupied, origin_of(object), footprint);
        if (destination.has_value()) {
            origin_of(object) = *destination;
            occupy(occupied, *destination, footprint);
            survivors.push_back(std::move(object));
            ++report.relocated;
        } else {
            history.destroyed_objects.push_back(destroyed_record(object));
            ++report.destroyed;
        }
    }
    objects = std::move(survivors);
}

}  // namespace

bool valid_site_digest(const SiteDigest& digest, const rules::Ruleset& ruleset) noexcept {
    if (!time::is_representable(digest.unload_tick)) {
        return false;
    }
    SitePersistentLayer persistent{digest.objects};
    CityBuildState state{digest.city_buildings, digest.pending, digest.economy,
                         digest.migration};
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

SiteMigrationReport migrate_site_digest(SiteDigest& digest,
                                         const SiteSkeleton& new_skeleton,
                                         const rules::Ruleset& ruleset) {
    if (!new_skeleton.valid_layout()) {
        throw std::invalid_argument{"骨架遷移要求有效的新 Site 骨架"};
    }
    const auto new_hash = hash_site_skeleton(new_skeleton);
    if (digest.skeleton_hash == new_hash) {
        return {};
    }
    if (!valid_site_digest(digest, ruleset)) {
        throw std::runtime_error{"骨架遷移收到無效 SiteDigest"};
    }

    SiteMigrationReport report{.applied = true};
    std::array<std::uint8_t, kSiteTileCount> occupied{};
    migrate_group(
        digest.objects, new_skeleton, occupied,
        [](auto& building) -> SiteXY& { return building.tile; },
        [](const auto&) { return Footprint{}; },
        [](const PersistentBuilding& building) {
            return SiteMigrationDestroyedObject{
                .kind = SiteMigrationObjectKind::PersistentBuilding,
                .definition_id = {},
                .former_coordinate = building.tile,
                .persistent_type = building.type,
                .persistent_state = building.state,
                .aging_seconds = building.aging_seconds,
            };
        },
        digest.migration, report);
    migrate_group(
        digest.city_buildings, new_skeleton, occupied,
        [](auto& building) -> SiteXY& { return building.origin; },
        [&](const auto& building) { return city_footprint(building.definition_id, ruleset); },
        [](const CityBuilding& building) {
            return SiteMigrationDestroyedObject{
                .kind = SiteMigrationObjectKind::CityBuilding,
                .definition_id = building.definition_id,
                .former_coordinate = building.origin,
            };
        },
        digest.migration, report);
    migrate_group(
        digest.pending, new_skeleton, occupied,
        [](auto& construction) -> SiteXY& { return construction.origin; },
        [&](const auto& construction) {
            return city_footprint(construction.definition_id, ruleset);
        },
        [](const PendingConstruction& construction) {
            return SiteMigrationDestroyedObject{
                .kind = SiteMigrationObjectKind::PendingConstruction,
                .definition_id = construction.definition_id,
                .former_coordinate = construction.origin,
                .remaining_hours = construction.remaining_hours,
            };
        },
        digest.migration, report);

    const auto old_hash = digest.skeleton_hash;
    digest.skeleton_hash = new_hash;
    digest.migration.events.push_back({
        .old_skeleton_hash = old_hash,
        .new_skeleton_hash = new_hash,
        .retained = report.retained,
        .relocated = report.relocated,
        .destroyed = report.destroyed,
        .narrative = "地貌異變重塑了城區：保留 " + std::to_string(report.retained) +
                     " 個物件，就近搬移 " + std::to_string(report.relocated) +
                     " 個，另有 " + std::to_string(report.destroyed) + " 個毀於災變。",
    });
    report.events_generated = 1;
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
                         digest.economy, std::move(digest.migration)};
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
    site.reg.emplace_or_replace<CityBuildState>(entity, std::move(state));
    site.reg.remove<SiteDigest>(entity);
    return report;
}

}  // namespace aetheria::site
