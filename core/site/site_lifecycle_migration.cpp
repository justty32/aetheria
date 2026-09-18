// SiteDigest 骨架遷移：依物件類別保留、搬移或記錄毀壞。

#include "core/site/site_lifecycle.h"
#include "core/site/site_lifecycle_migration_detail.h"

#include <array>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace aetheria::site {
namespace {

using detail::Footprint;
using detail::can_place;
using detail::nearest_legal_origin;
using detail::occupy;

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

}  // namespace aetheria::site
