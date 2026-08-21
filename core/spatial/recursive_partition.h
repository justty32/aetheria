#pragma once

// recursive_partition.h：Site 街廓與 Local 房間共用的有界遞迴矩形切分。

#include <cstdint>
#include <vector>

namespace aetheria::spatial {

struct PartitionRect {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint16_t width{};
    std::uint16_t height{};

    [[nodiscard]] constexpr std::uint32_t area() const noexcept {
        return static_cast<std::uint32_t>(width) * height;
    }
    constexpr bool operator==(const PartitionRect&) const noexcept = default;
};

// PartitionCut 的 coordinate 是垂直切分的 x 或水平切分的 y；start/extent
// 描述另一軸。separator_extent=0 時它是兩矩形間的邊，=1 時是街道格。
struct PartitionCut {
    bool vertical{};
    std::uint16_t coordinate{};
    std::uint16_t start{};
    std::uint16_t extent{};

    constexpr bool operator==(const PartitionCut&) const noexcept = default;
};

struct RecursivePartitionConfig {
    std::uint8_t max_depth{};
    std::uint8_t cut_min_percent{};
    std::uint8_t cut_max_percent{};
    std::uint8_t min_extent{};
    std::uint8_t separator_extent{};
};

struct RecursivePartition {
    std::vector<PartitionRect> leaves;
    std::vector<PartitionCut> cuts;

    bool operator==(const RecursivePartition&) const = default;
};

[[nodiscard]] RecursivePartition partition_rect(PartitionRect root, std::uint64_t seed,
                                                RecursivePartitionConfig config);

}  // namespace aetheria::spatial
