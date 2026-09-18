#pragma once

#include "core/local/local_buildings.h"
#include "core/local/local_underground.h"
#include "sim/debug_canvas.h"

namespace aetheria::sim::detail::local_viewer {

inline constexpr std::int32_t kTilePixels = 8;

void paint_ground(DebugCanvas& canvas, const local::LocalTiles& tiles,
                  const rules::Ruleset& ruleset);

void paint_edges(DebugCanvas& canvas, const local::LocalTiles& tiles,
                 const rules::Ruleset& ruleset);

void paint_rooms(DebugCanvas& canvas, const local::BuildingLocalSkeleton& skeleton, std::int8_t z);

void paint_rooms(DebugCanvas& canvas, const local::UndergroundLocalSkeleton& skeleton,
                 std::int8_t z);

void paint_occupants(DebugCanvas& canvas, const local::LocalTiles& tiles,
                     const local::BuildingLocalSkeleton* building, std::int8_t z);

void write_local_layers(const std::map<std::int8_t, local::LocalTiles>& layers,
                        const local::BuildingLocalSkeleton* building,
                        const local::UndergroundLocalSkeleton* underground, std::string_view zoning,
                        std::string_view z, const rules::Ruleset& ruleset,
                        const std::filesystem::path& directory);

} // namespace aetheria::sim::detail::local_viewer
