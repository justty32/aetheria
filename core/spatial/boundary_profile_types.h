#pragma once

// boundary_profile_types.h：共用邊界資料與規範識別。

#include "core/rules/ruleset.h"
#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace aetheria::spatial {

inline constexpr std::uint32_t kBoundarySampleCount = 64;
inline constexpr std::uint64_t kBoundaryEdgeSalt = UINT64_C(0x4A8E9137D52CB60F);
inline constexpr std::uint64_t kBoundaryCornerSalt = UINT64_C(0x8D1C6B42F370A95E);

struct BoundaryCrossing {
    std::uint8_t pos{};
    std::uint8_t width{};
    rules::EdgeId kind{};

    constexpr bool operator==(const BoundaryCrossing&) const noexcept = default;
};

struct BoundaryProfile {
    std::array<std::uint16_t, kBoundarySampleCount> elevation{};
    std::array<rules::GroundId, kBoundarySampleCount> ground{};
    std::array<std::uint8_t, kBoundarySampleCount> water_depth{};
    std::array<rules::EdgeId, kBoundarySampleCount> edges{};
    std::vector<BoundaryCrossing> crossings;

    bool operator==(const BoundaryProfile&) const = default;
};

[[nodiscard]] constexpr std::uint64_t canonical_edge_id(std::size_t first,
                                                        std::size_t second,
                                                        std::uint32_t width) noexcept {
    const auto low = std::min(first, second);
    const bool south = std::max(first, second) - low == width;
    return static_cast<std::uint64_t>(low) * 2U + static_cast<std::uint64_t>(south);
}

[[nodiscard]] constexpr std::uint64_t canonical_corner_id(std::uint32_t x,
                                                          std::uint32_t y,
                                                          std::uint32_t width) noexcept {
    return static_cast<std::uint64_t>(y) * (width + 1U) + x;
}

}  // namespace aetheria::spatial
