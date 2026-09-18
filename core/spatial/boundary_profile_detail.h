#pragma once

// boundary_profile_detail.h：邊界角點選取與取樣；由 boundary_profile.h 引入。

namespace aetheria::spatial {

namespace detail {

struct CornerSample {
    std::uint16_t elevation{};
    rules::GroundId ground{};
    std::uint8_t water_depth{};
};

[[nodiscard]] constexpr std::array<std::uint32_t, 4> edge_corners(
    std::uint32_t x, std::uint32_t y, BoundarySide side) noexcept {
    switch (side) {
    case BoundarySide::North:
        return {x, y, x + 1U, y};
    case BoundarySide::East:
        return {x + 1U, y, x + 1U, y + 1U};
    case BoundarySide::South:
        return {x, y + 1U, x + 1U, y + 1U};
    case BoundarySide::West:
        return {x, y, x, y + 1U};
    }
    return {};
}

template <typename Source>
[[nodiscard]] CornerSample sample_corner(const Source& source, std::uint32_t corner_x,
                                         std::uint32_t corner_y, std::uint64_t seed) {
    std::array<std::size_t, 4> candidates{};
    std::size_t count{};
    for (std::int32_t dy = -1; dy <= 0; ++dy) {
        for (std::int32_t dx = -1; dx <= 0; ++dx) {
            const auto x = static_cast<std::int32_t>(corner_x) + dx;
            const auto y = static_cast<std::int32_t>(corner_y) + dy;
            if (x >= 0 && y >= 0 && x < static_cast<std::int32_t>(source.width()) &&
                y < static_cast<std::int32_t>(source.height())) {
                candidates[count++] = source.index(static_cast<std::uint32_t>(x),
                                                   static_cast<std::uint32_t>(y));
            }
        }
    }
    if (count == 0) {
        throw std::logic_error{"邊界角點沒有相鄰父層 tile"};
    }
    std::uint64_t elevation_sum{};
    for (std::size_t index = 0; index < count; ++index) {
        elevation_sum += source.elevation(candidates[index]);
    }
    const auto selected = candidates[static_cast<std::size_t>(seed % count)];
    return {static_cast<std::uint16_t>(elevation_sum / count), source.ground(selected),
            source.water_depth(selected)};
}

}  // namespace detail

}  // namespace aetheria::spatial
