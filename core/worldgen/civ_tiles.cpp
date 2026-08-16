#include "core/worldgen/civ_tiles.h"

#include <algorithm>
#include <stdexcept>

namespace aetheria::worldgen::detail {
namespace {

[[nodiscard]] std::size_t checked_count(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0 || width > static_cast<std::uint32_t>(INT16_MAX) ||
        height > static_cast<std::uint32_t>(INT16_MAX)) {
        throw std::invalid_argument{"文明生成尺寸必須落在 1..32767"};
    }
    return static_cast<std::size_t>(width) * height;
}

void install_rivers(world::RegionTiles& tiles, const RiverStageOutput& rivers,
                    const RegionDefinitionIds& definitions) {
    for (std::size_t index = 0; index < rivers.river_class.size(); ++index) {
        const auto target_raw = rivers.downstream[index];
        const auto river_class = rivers.river_class[index];
        if (river_class == 0 || target_raw < 0) {
            continue;
        }
        const auto target = static_cast<std::size_t>(target_raw);
        if (target >= rivers.river_class.size()) {
            throw std::runtime_error{"河流 downstream 超出文明階段輸入"};
        }
        const auto edge = river_class == 3   ? definitions.great_river
                          : river_class == 2 ? definitions.river
                                             : definitions.stream;
        tiles.set_edge(coordinate(index, tiles.width), coordinate(target, tiles.width), edge);
    }
}

}  // namespace

std::array<std::size_t, 4> neighbors(std::size_t index, std::uint32_t width,
                                     std::uint32_t height) noexcept {
    const auto x = index % width;
    const auto y = index / width;
    return {y > 0 ? index - width : kMissing, x + 1 < width ? index + 1 : kMissing,
            y + 1 < height ? index + width : kMissing, x > 0 ? index - 1 : kMissing};
}

world::RegionXY coordinate(std::size_t index, std::uint32_t width) noexcept {
    return {static_cast<std::int16_t>(index % width), static_cast<std::int16_t>(index / width)};
}

void require_civilization_inputs(const QuantizedElevation& elevation,
                                 const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                                 const BiomeStageOutput& biome,
                                 const FeatureStageOutput& features) {
    const auto count = checked_count(elevation.width, elevation.height);
    if (climate.width != elevation.width || climate.height != elevation.height ||
        rivers.width != elevation.width || rivers.height != elevation.height ||
        biome.width != elevation.width || biome.height != elevation.height ||
        features.width != elevation.width || features.height != elevation.height ||
        elevation.meters.size() != count || elevation.land.size() != count ||
        climate.temperature_tenths.size() != count || rivers.downstream.size() != count ||
        rivers.river_class.size() != count || rivers.moisture.size() != count ||
        rivers.lake.size() != count || biome.terrain.size() != count ||
        biome.relief.size() != count || features.feature.size() != count) {
        throw std::invalid_argument{"文明生成階段輸入尺寸不一致"};
    }
}

world::RegionTiles
make_base_tiles(const QuantizedElevation& elevation, const ClimateStageOutput& climate,
                const RiverStageOutput& rivers, const BiomeStageOutput& biome,
                const FeatureStageOutput& features, const RegionDefinitionIds& definitions) {
    world::RegionTiles tiles{elevation.width, elevation.height};
    tiles.base = biome.terrain;
    tiles.relief = biome.relief;
    tiles.feature = features.feature;
    tiles.elevation = elevation.meters;
    for (std::size_t index = 0; index < elevation.meters.size(); ++index) {
        tiles.temperature[index] = static_cast<std::uint8_t>(std::clamp<std::int32_t>(
            (static_cast<std::int32_t>(climate.temperature_tenths[index]) + 500) * 255 / 1000, 0,
            UINT8_MAX));
        tiles.moisture[index] = static_cast<std::uint8_t>(rivers.moisture[index] / 257U);
    }
    std::fill(tiles.edges.begin(), tiles.edges.end(), definitions.no_edge);
    install_rivers(tiles, rivers, definitions);
    return tiles;
}

}  // namespace aetheria::worldgen::detail
