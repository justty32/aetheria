#pragma once

// local_underground.h：L3 路線 C 的礦坑、地城與拆除式遺跡。
// 地下層仍屬同一 Local zone，structure def 同時指定種類與深度。

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "core/local/local_tiles.h"
#include "core/spatial/recursive_partition.h"

namespace aetheria::local {

struct UndergroundRoom {
    spatial::PartitionRect footprint;
    std::int8_t z{};

    constexpr bool operator==(const UndergroundRoom&) const noexcept = default;
};

// destination_outside→destination_inside
// 是通往目的房間的唯一門檻，可供驗證故障注入。
struct UndergroundCorridor {
    std::int8_t z{};
    std::uint16_t from_room{};
    std::uint16_t to_room{};
    LocalXY destination_outside;
    LocalXY destination_inside;
    std::uint16_t tile_count{};

    constexpr bool operator==(const UndergroundCorridor&) const noexcept = default;
};

// 單筆 link 表達同格相鄰 z 層的雙向可通行關係；兩端都必須標成 Stairs。
struct UndergroundVerticalLink {
    LocalXY tile;
    std::int8_t upper_z{};
    std::int8_t lower_z{};

    constexpr bool operator==(const UndergroundVerticalLink&) const noexcept = default;
};

struct UndergroundLocalSkeleton {
    rules::UndergroundKind kind{rules::UndergroundKind::None};
    std::uint8_t depth{};
    LocalXY entrance;
    std::map<std::int8_t, LocalTiles> layers;
    // 只含負 z 層；1 表示生成器挖開或遺跡可站立的格。
    std::map<std::int8_t, std::vector<std::uint8_t>> excavated;
    std::vector<UndergroundRoom> rooms;
    std::vector<UndergroundCorridor> corridors;
    std::vector<UndergroundVerticalLink> vertical_links;
    std::uint32_t excavated_count{};
    std::uint32_t ruin_original_segments{};
    std::uint32_t ruin_removed_segments{};

    [[nodiscard]] bool valid_layout() const noexcept;
    bool operator==(const UndergroundLocalSkeleton&) const = default;
};

[[nodiscard]] UndergroundLocalSkeleton build_underground_local_skeleton(
    const LocalSlowVars& slow, std::uint64_t local_seed, const rules::Ruleset& ruleset);

// 生成後驗證使用 flood fill；生成本身不呼叫尋路或全圖 A*。
[[nodiscard]] std::size_t count_unreachable_underground_rooms(
    const UndergroundLocalSkeleton& skeleton, const rules::Ruleset& ruleset);
[[nodiscard]] bool all_underground_tiles_reachable(const UndergroundLocalSkeleton& skeleton,
                                                   const rules::Ruleset& ruleset);
[[nodiscard]] bool valid_underground_invariants(const UndergroundLocalSkeleton& skeleton,
                                                const rules::Ruleset& ruleset) noexcept;
[[nodiscard]] std::uint64_t hash_underground_local_skeleton(
    const UndergroundLocalSkeleton& skeleton) noexcept;

}  // namespace aetheria::local
