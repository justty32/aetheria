#include "sim/local_viewer.h"

#include "core/local/local_buildings.h"
#include "core/local/local_tiles.h"
#include "core/local/local_underground.h"
#include "sim/debug_canvas.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "sim/local_viewer_detail.h"

namespace aetheria::sim::detail::local_viewer {

namespace {

constexpr std::uint32_t kImageExtent = local::kLocalWidth * kTilePixels + 1U;

[[nodiscard]] std::vector<std::int8_t>
selected_layers(std::string_view requested,
                const std::map<std::int8_t, local::LocalTiles>& layers) {
    if (requested == "all") {
        std::vector<std::int8_t> result;
        for (const auto& [z, tiles] : layers) {
            static_cast<void>(tiles);
            result.push_back(z);
        }
        return result;
    }
    std::size_t parsed{};
    const auto value = std::stoi(std::string{requested}, &parsed);
    if (parsed != requested.size() || value < std::numeric_limits<std::int8_t>::min() ||
        value > std::numeric_limits<std::int8_t>::max() ||
        !layers.contains(static_cast<std::int8_t>(value))) {
        throw std::invalid_argument{"gen local 的 z 必須是現有層號或 all"};
    }
    return {static_cast<std::int8_t>(value)};
}

[[nodiscard]] std::string z_name(std::int8_t z) {
    if (z < 0) {
        return "m" + std::to_string(-static_cast<std::int16_t>(z));
    }
    return z > 0 ? "p" + std::to_string(z) : "0";
}

} // namespace

void write_local_layers(const std::map<std::int8_t, local::LocalTiles>& layers,
                        const local::BuildingLocalSkeleton* building,
                        const local::UndergroundLocalSkeleton* underground, std::string_view zoning,
                        std::string_view z, const rules::Ruleset& ruleset,
                        const std::filesystem::path& directory) {
    for (const auto selected : selected_layers(z, layers)) {
        const auto& tiles = layers.at(selected);
        const auto stem = "local-" + std::string{zoning} + "-z" + z_name(selected);

        DebugCanvas ground{kImageExtent, kImageExtent, {24, 24, 28}};
        paint_ground(ground, tiles, ruleset);
        ground.write_png(directory / (stem + "-ground.png"));

        DebugCanvas edges{kImageExtent, kImageExtent, {24, 24, 28}};
        paint_ground(edges, tiles, ruleset);
        paint_edges(edges, tiles, ruleset);
        edges.write_png(directory / (stem + "-edges.png"));

        DebugCanvas rooms{kImageExtent, kImageExtent, {24, 24, 28}};
        if (building != nullptr) {
            paint_rooms(rooms, *building, selected);
        } else if (underground != nullptr) {
            paint_rooms(rooms, *underground, selected);
        } else {
            paint_ground(rooms, tiles, ruleset);
        }
        paint_edges(rooms, tiles, ruleset);
        rooms.write_png(directory / (stem + "-rooms.png"));

        DebugCanvas occupants{kImageExtent, kImageExtent, {24, 24, 28}};
        paint_ground(occupants, tiles, ruleset);
        paint_occupants(occupants, tiles, building, selected);
        paint_edges(occupants, tiles, ruleset);
        occupants.write_png(directory / (stem + "-occupants.png"));
    }
}

} // namespace aetheria::sim::detail::local_viewer
