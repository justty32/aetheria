#include "core/world/region_tiles.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace aetheria::world {
namespace {

enum class Direction : std::size_t {
    North,
    East,
    South,
    West,
};

struct DirectedEdge {
    Direction from_a;
    Direction from_b;
};

[[nodiscard]] DirectedEdge directions(RegionXY a, RegionXY b) {
    const auto dx = static_cast<int>(b.x) - static_cast<int>(a.x);
    const auto dy = static_cast<int>(b.y) - static_cast<int>(a.y);
    if (dx == 0 && dy == -1) {
        return {Direction::North, Direction::South};
    }
    if (dx == 1 && dy == 0) {
        return {Direction::East, Direction::West};
    }
    if (dx == 0 && dy == 1) {
        return {Direction::South, Direction::North};
    }
    if (dx == -1 && dy == 0) {
        return {Direction::West, Direction::East};
    }
    throw std::runtime_error{"set_edge 只接受四鄰接的兩個 RegionXY"};
}

[[nodiscard]] std::size_t edge_offset(std::size_t tile_index, Direction direction) noexcept {
    return tile_index * 4U + static_cast<std::size_t>(direction);
}

template <typename Value>
[[nodiscard]] std::size_t bytes(const std::vector<Value>& values) noexcept {
    return values.size() * sizeof(Value);
}

}  // namespace

RegionTiles::RegionTiles(std::uint32_t grid_width, std::uint32_t grid_height)
    : width{grid_width}, height{grid_height} {
    AETH_CHECK(width > 0 && height > 0);
    const auto count64 = static_cast<std::uint64_t>(width) * height;
    AETH_CHECK(count64 <= std::numeric_limits<std::size_t>::max() / 4U);
    const auto count = static_cast<std::size_t>(count64);
    base.resize(count);
    relief.resize(count);
    feature.resize(count);
    temperature.resize(count);
    moisture.resize(count);
    elevation.resize(count);
    edges.resize(count * 4U);
    owner.resize(count);
    site.resize(count);
}

std::size_t RegionTiles::tile_count() const noexcept {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

std::size_t RegionTiles::index_of(RegionXY coordinate) const {
    if (coordinate.x < 0 || coordinate.y < 0 ||
        static_cast<std::uint32_t>(coordinate.x) >= width ||
        static_cast<std::uint32_t>(coordinate.y) >= height) {
        throw std::out_of_range{"RegionXY 超出 RegionTiles 邊界"};
    }
    return static_cast<std::size_t>(coordinate.y) * width +
           static_cast<std::size_t>(coordinate.x);
}

bool RegionTiles::valid_layout() const noexcept {
    const auto count64 = static_cast<std::uint64_t>(width) * height;
    if (width == 0 || height == 0 ||
        count64 > std::numeric_limits<std::size_t>::max() / 4U) {
        return false;
    }
    const auto count = static_cast<std::size_t>(count64);
    return base.size() == count && relief.size() == count &&
           feature.size() == count && temperature.size() == count && moisture.size() == count &&
           elevation.size() == count && edges.size() == count * 4U && owner.size() == count &&
           site.size() == count;
}

void RegionTiles::set_edge(RegionXY a, RegionXY b, rules::EdgeId edge_id) {
    const auto pair = directions(a, b);
    const auto a_index = index_of(a);
    const auto b_index = index_of(b);
    edges.at(edge_offset(a_index, pair.from_a)) = edge_id;
    edges.at(edge_offset(b_index, pair.from_b)) = edge_id;
}

rules::EdgeId RegionTiles::edge_between(RegionXY a, RegionXY b) const {
    const auto pair = directions(a, b);
    return edges.at(edge_offset(index_of(a), pair.from_a));
}

std::size_t RegionTiles::edge_storage_bytes() const noexcept { return bytes(edges); }

std::size_t RegionTiles::dynamic_storage_bytes() const noexcept {
    return bytes(base) + bytes(relief) + bytes(feature) + bytes(temperature) + bytes(moisture) +
           bytes(elevation) + bytes(edges) + bytes(owner) + bytes(site);
}

}  // namespace aetheria::world
