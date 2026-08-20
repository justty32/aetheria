#include "sim/stage_dump.h"

#include "sim/pgm_writer.h"

namespace aetheria::sim {

void dump_region_stages(const worldgen::RegionBuildResult& result,
                        const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    write_pgm(directory / "01-plates.pgm", result.plates.width, result.plates.height,
              worldgen::grayscale(result.plates));
    write_pgm(directory / "02-height.pgm", result.height.width, result.height.height,
              worldgen::grayscale(result.height));
    write_pgm(directory / "03-erosion.pgm", result.erosion.width, result.erosion.height,
              worldgen::grayscale(result.erosion));
    write_pgm(directory / "04-climate.pgm", result.climate.width, result.climate.height,
              worldgen::grayscale(result.climate));
    write_pgm(directory / "05-rivers.pgm", result.rivers.width, result.rivers.height,
              worldgen::grayscale(result.rivers));
    write_pgm(directory / "06-biome.pgm", result.biome.width, result.biome.height,
              worldgen::grayscale(result.biome));
    write_pgm(directory / "07-features.pgm", result.features.width, result.features.height,
              worldgen::grayscale(result.features));
    write_pgm(directory / "08-history.pgm", result.history.features.width,
              result.history.features.height, worldgen::grayscale(result.history));
    write_pgm(directory / "09-cities.pgm", result.cities.width, result.cities.height,
              worldgen::grayscale(result.cities));
    write_pgm(directory / "10-roads.pgm", result.roads.width, result.roads.height,
              worldgen::grayscale(result.roads));
    write_pgm(directory / "11-portals.pgm", result.portals.width, result.portals.height,
              worldgen::grayscale(result.portals));
    write_pgm(directory / "12-factions.pgm", result.factions.width, result.factions.height,
              worldgen::grayscale(result.factions));
}

}  // namespace aetheria::sim
