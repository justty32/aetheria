#pragma once

// site_wilderness.h 定義荒野共享邊界、W1～W6 程序輸出與非持久實體。

#include "core/site/site_projection.h"

#include <array>
#include <cstdint>
#include <vector>

namespace aetheria::zone {
struct Zone;
}

namespace aetheria::site {

struct BoundaryCrossing {
    std::uint8_t pos{};
    std::uint8_t width{};
    rules::EdgeId kind{};

    constexpr bool operator==(const BoundaryCrossing&) const noexcept = default;
};

struct BoundaryProfile {
    std::array<std::uint16_t, kSiteWidth> elevation{};
    std::array<rules::GroundId, kSiteWidth> ground{};
    std::array<std::uint8_t, kSiteWidth> water_depth{};
    std::array<rules::EdgeId, kSiteWidth> edges{};
    std::vector<BoundaryCrossing> crossings;

    bool operator==(const BoundaryProfile&) const = default;
};

// 四個 profile 一律以規範方向排列：垂直邊北→南，水平邊西→東。
struct WildernessSlowVars {
    SiteSlowVars local;
    std::array<BoundaryProfile, 4> boundaries;
};

struct WildernessSkeleton {
    rules::TerrainId source_base{};
    rules::ReliefId source_relief{};
    rules::FeatureId source_feature{};
    SiteSkeleton terrain;
    std::array<BoundaryProfile, 4> boundaries;
    std::vector<SiteXY> vegetation;
    std::vector<SiteXY> portals;
    std::vector<SiteBlock> ruin_structures;
    std::uint16_t river_path_count{};
    std::uint16_t road_path_count{};
    std::uint16_t lake_count{};
    std::uint16_t bridge_count{};

    [[nodiscard]] bool valid_layout() const noexcept;
    bool operator==(const WildernessSkeleton&) const = default;
};

struct WildernessPopulation {
    std::vector<SiteXY> resource_points;
    std::vector<SiteXY> encounter_points;
    std::vector<SiteXY> traveler_points;

    bool operator==(const WildernessPopulation&) const = default;
};

struct WildernessSite {
    WildernessSkeleton skeleton;
    WildernessPopulation population;

    [[nodiscard]] bool valid_layout() const noexcept;
    bool operator==(const WildernessSite&) const = default;
};

// 下列 component 只存在於 live Site registry，刻意不登記 AllComponents。
struct SitePosition {
    SiteXY tile;
};
struct WildernessVegetation {
    rules::FeatureId source{};
};
struct WildernessResourcePoint {
    rules::FeatureId source{};
};
struct WildernessEncounterPoint {
    world::FactionId authority{};
};
struct WildernessTravelerPoint {};
struct WildernessPortal {
    std::int8_t destination_z{-1};
};
struct WildernessRuinStructure {
    std::uint16_t width{};
    std::uint16_t height{};
};

[[nodiscard]] WildernessSlowVars project_wilderness_slow_vars(
    const world::RegionTiles& tiles, world::RegionXY coordinate, std::uint64_t world_seed,
    std::uint32_t region_id, const rules::Ruleset& ruleset);

[[nodiscard]] WildernessSkeleton build_wilderness_skeleton(
    const WildernessSlowVars& slow, std::uint64_t site_seed, const rules::Ruleset& ruleset);

[[nodiscard]] WildernessSite populate_wilderness(WildernessSkeleton skeleton,
                                                 const SiteFastVars& fast,
                                                 std::uint64_t site_seed,
                                                 const rules::Ruleset& ruleset);

[[nodiscard]] WildernessSite generate_wilderness_site(
    const world::RegionTiles& tiles, world::RegionXY coordinate, std::uint64_t world_seed,
    std::uint32_t region_id, const rules::Ruleset& ruleset);

[[nodiscard]] std::uint64_t hash_wilderness_site(const WildernessSite& site) noexcept;

void install_wilderness_entities(zone::Zone& target, const WildernessSite& wilderness,
                                 rules::FeatureId feature, world::FactionId owner);

}  // namespace aetheria::site
