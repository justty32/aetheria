#pragma once

// local_buildings.h：L3 路線 A 的街廓房屋、房間、家具、居民統計與垂直層。

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "core/local/local_tiles.h"
#include "core/spatial/recursive_partition.h"

namespace aetheria::local {

struct LocalHouse {
    spatial::PartitionRect footprint;
    spatial::BoundarySide frontage{spatial::BoundarySide::North};
    std::uint16_t resident_count{};
    bool has_cellar{};
    bool has_upper_floor{};
    bool residents_materialized{};

    constexpr bool operator==(const LocalHouse&) const noexcept = default;
};

struct LocalRoom {
    spatial::PartitionRect footprint;
    std::int8_t z{};
    std::uint16_t house{};
    rules::LocalRoomKind kind{rules::LocalRoomKind::Bedroom};

    constexpr bool operator==(const LocalRoom&) const noexcept = default;
};

struct FurniturePlacement {
    LocalXY tile;
    std::int8_t z{};
    std::uint16_t room{};
    rules::FurnitureDefId def{};
    EntityId entity{};

    constexpr bool operator==(const FurniturePlacement&) const noexcept = default;
};

struct VerticalLink {
    LocalXY tile;
    std::int8_t from_z{};
    std::int8_t to_z{};

    constexpr bool operator==(const VerticalLink&) const noexcept = default;
};

struct BuildingLocalSkeleton {
    std::map<std::int8_t, LocalTiles> layers;
    std::vector<std::uint16_t> elevation;
    std::array<spatial::BoundaryProfile, 4> boundaries;
    std::vector<LocalHouse> houses;
    std::vector<LocalRoom> rooms;
    std::vector<FurniturePlacement> furniture;
    std::vector<VerticalLink> vertical_links;
    std::uint32_t resident_statistics{};
    std::uint16_t door_count{};
    std::uint16_t window_count{};
    std::uint16_t ambient_resident_count{};

    [[nodiscard]] std::size_t entity_count() const noexcept;
    [[nodiscard]] bool valid_layout() const noexcept;
    bool operator==(const BuildingLocalSkeleton&) const = default;
};

[[nodiscard]] BuildingLocalSkeleton build_building_local_skeleton(const LocalSlowVars& slow,
                                                                  std::uint64_t local_seed,
                                                                  const rules::Ruleset& ruleset);

// 只有玩家進入指定房屋才把統計居民具象化；重複進入不會重複生成。
void materialize_ambient_residents(BuildingLocalSkeleton& skeleton, std::uint16_t house,
                                   std::uint64_t local_seed);

[[nodiscard]] bool valid_building_invariants(const BuildingLocalSkeleton& skeleton,
                                             const rules::Ruleset& ruleset) noexcept;

[[nodiscard]] std::uint64_t hash_building_local_skeleton(
    const BuildingLocalSkeleton& skeleton) noexcept;

}  // namespace aetheria::local
