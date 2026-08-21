#include "core/spatial/recursive_partition.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "core/worldgen/region_seed.h"

namespace aetheria::spatial {
namespace {

void split(RecursivePartition& result, PartitionRect rect, std::uint8_t depth, std::uint64_t seed,
           const RecursivePartitionConfig& config) {
    const bool vertical =
        rect.width > rect.height ||
        (rect.width == rect.height && (worldgen::splitmix64(seed) & UINT64_C(1)) != 0);
    const auto extent = vertical ? rect.width : rect.height;
    const auto required =
        static_cast<std::uint16_t>(config.min_extent * 2U + config.separator_extent);
    if (depth >= config.max_depth || extent < required) {
        result.leaves.push_back(rect);
        return;
    }

    const auto random = worldgen::splitmix64(seed ^ depth);
    const auto percent_span =
        static_cast<std::uint16_t>(config.cut_max_percent - config.cut_min_percent + 1U);
    auto percent = static_cast<std::uint16_t>(config.cut_min_percent + random % percent_span);
    if ((random & UINT64_C(0x100)) != 0) {
        percent = static_cast<std::uint16_t>(100U - percent);
    }
    auto first_extent = static_cast<std::uint16_t>(extent * percent / 100U);
    first_extent = std::clamp<std::uint16_t>(
        first_extent, config.min_extent,
        static_cast<std::uint16_t>(extent - config.min_extent - config.separator_extent));
    const auto second_extent =
        static_cast<std::uint16_t>(extent - first_extent - config.separator_extent);

    if (vertical) {
        const auto coordinate = static_cast<std::uint16_t>(rect.x + first_extent);
        result.cuts.push_back({true, coordinate, rect.y, rect.height});
        split(result, {rect.x, rect.y, first_extent, rect.height}, depth + 1U,
              worldgen::splitmix64(seed ^ UINT64_C(0xA1)), config);
        split(result,
              {static_cast<std::uint16_t>(coordinate + config.separator_extent), rect.y,
               second_extent, rect.height},
              depth + 1U, worldgen::splitmix64(seed ^ UINT64_C(0xB2)), config);
    } else {
        const auto coordinate = static_cast<std::uint16_t>(rect.y + first_extent);
        result.cuts.push_back({false, coordinate, rect.x, rect.width});
        split(result, {rect.x, rect.y, rect.width, first_extent}, depth + 1U,
              worldgen::splitmix64(seed ^ UINT64_C(0xC3)), config);
        split(result,
              {rect.x, static_cast<std::uint16_t>(coordinate + config.separator_extent), rect.width,
               second_extent},
              depth + 1U, worldgen::splitmix64(seed ^ UINT64_C(0xD4)), config);
    }
}

}  // namespace

RecursivePartition partition_rect(PartitionRect root, std::uint64_t seed,
                                  RecursivePartitionConfig config) {
    if (root.width == 0 || root.height == 0 || config.max_depth == 0 || config.min_extent == 0 ||
        config.cut_min_percent == 0 || config.cut_min_percent > config.cut_max_percent ||
        config.cut_max_percent >= 50U || config.separator_extent > 1U) {
        throw std::invalid_argument{"遞迴矩形切分參數無效"};
    }
    RecursivePartition result;
    split(result, root, 0, seed, config);
    return result;
}

}  // namespace aetheria::spatial
