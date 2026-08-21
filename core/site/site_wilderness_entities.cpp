#include "core/site/site_wilderness.h"

#include "core/zone/zone.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace aetheria::site {
namespace {

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
}

template <typename Value> void hash_integer(std::uint64_t& hash, Value value) noexcept {
    const auto bits = static_cast<std::make_unsigned_t<Value>>(value);
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits >> (byte * 8U)));
    }
}

void hash_points(std::uint64_t& hash, const std::vector<SiteXY>& points) noexcept {
    hash_integer(hash, static_cast<std::uint64_t>(points.size()));
    for (const auto point : points) {
        hash_integer(hash, point.x);
        hash_integer(hash, point.y);
    }
}

}  // namespace

std::uint64_t hash_wilderness_site(const WildernessSite& site) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_integer(hash, hash_site_skeleton(site.skeleton.terrain));
    hash_integer(hash, rules::value_of(site.skeleton.source_base));
    hash_integer(hash, rules::value_of(site.skeleton.source_relief));
    hash_integer(hash, rules::value_of(site.skeleton.source_feature));
    hash_points(hash, site.skeleton.vegetation);
    hash_points(hash, site.skeleton.portals);
    hash_integer(hash, site.skeleton.river_path_count);
    hash_integer(hash, site.skeleton.road_path_count);
    hash_integer(hash, site.skeleton.lake_count);
    hash_integer(hash, site.skeleton.bridge_count);
    hash_integer(hash, static_cast<std::uint64_t>(site.skeleton.ruin_structures.size()));
    for (const auto& block : site.skeleton.ruin_structures) {
        hash_integer(hash, block.origin.x);
        hash_integer(hash, block.origin.y);
        hash_integer(hash, block.width);
        hash_integer(hash, block.height);
    }
    hash_points(hash, site.population.resource_points);
    hash_points(hash, site.population.encounter_points);
    hash_points(hash, site.population.traveler_points);
    return hash;
}

void install_wilderness_entities(zone::Zone& target, const WildernessSite& wilderness,
                                 rules::FeatureId feature, world::FactionId owner) {
    for (const auto point : wilderness.skeleton.vegetation) {
        const auto entity = target.reg.create();
        target.reg.emplace<SitePosition>(entity, point);
        target.reg.emplace<WildernessVegetation>(entity, feature);
    }
    for (const auto point : wilderness.population.resource_points) {
        const auto entity = target.reg.create();
        target.reg.emplace<SitePosition>(entity, point);
        target.reg.emplace<WildernessResourcePoint>(entity, feature);
    }
    for (const auto point : wilderness.population.encounter_points) {
        const auto entity = target.reg.create();
        target.reg.emplace<SitePosition>(entity, point);
        target.reg.emplace<WildernessEncounterPoint>(entity, owner);
    }
    for (const auto point : wilderness.population.traveler_points) {
        const auto entity = target.reg.create();
        target.reg.emplace<SitePosition>(entity, point);
        target.reg.emplace<WildernessTravelerPoint>(entity);
    }
    for (const auto point : wilderness.skeleton.portals) {
        const auto entity = target.reg.create();
        target.reg.emplace<SitePosition>(entity, point);
        target.reg.emplace<WildernessPortal>(entity);
    }
    for (const auto& block : wilderness.skeleton.ruin_structures) {
        const auto entity = target.reg.create();
        target.reg.emplace<SitePosition>(entity, block.origin);
        target.reg.emplace<WildernessRuinStructure>(entity, block.width, block.height);
    }
}

}  // namespace aetheria::site
