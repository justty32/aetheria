#pragma once

// worldgen 測試共用的 fixture 與 helper（暫存目錄、規則資料覆寫等）。

#include "core/rules/ruleset.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aetheria::tests {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        static std::uint64_t serial{};
        path_ = std::filesystem::temp_directory_path() /
                ("aetheria-worldgen-test-" + std::to_string(++serial));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  private:
    std::filesystem::path path_;
};

inline void write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream stream{path};
    ASSERT_TRUE(stream.is_open());
    stream << text;
    ASSERT_TRUE(stream.good());
}

inline void copy_data_files(const std::filesystem::path& destination) {
    const auto source = std::filesystem::path{AETHERIA_SOURCE_DIR} / "data";
    for (const auto& entry : std::filesystem::recursive_directory_iterator{source}) {
        const auto target = destination / std::filesystem::relative(entry.path(), source);
        if (entry.is_directory()) {
            std::filesystem::create_directories(target);
        } else {
            std::filesystem::copy_file(entry.path(), target,
                                       std::filesystem::copy_options::overwrite_existing);
        }
    }
}

[[nodiscard]] inline rules::Ruleset ruleset_without_ocean() {
    TemporaryDirectory directory;
    write_text(directory.path() / "terrain.toml", R"(
[[defs]]
id = "terrain.grassland"
name_key = "terrain.grassland.name"
move_cost = 1
flags = 1
visual = "terrain/grassland"
yield = { food = 2, production = 1, wealth = 0, mana = 0 }
)");
    write_text(directory.path() / "relief.toml", R"(
[[defs]]
id = "relief.plain"
name_key = "relief.plain.name"
move_cost = 1
flags = 0
visual = "relief/plain"
)");
    write_text(directory.path() / "feature.toml", R"(
[[defs]]
id = "feature.none"
name_key = "feature.none.name"
move_cost = 1
flags = 0
visual = "feature/none"
)");
    write_text(directory.path() / "edges.toml", R"(
[[defs]]
id = "edge.none"
name_key = "edge.none.name"
move_cost = 1
flags = 0
visual = "edge/none"
[[defs]]
id = "edge.city_wall"
name_key = "wall"
move_cost = 1000
flags = 8
visual = "wall"
[[defs]]
id = "edge.city_gate"
name_key = "gate"
move_cost = 1
flags = 57
visual = "gate"
[[defs]]
id = "edge.city_wall_tower"
name_key = "tower"
move_cost = 1000
flags = 72
visual = "tower"
[[defs]]
id = "edge.city_moat"
name_key = "moat"
move_cost = 1000
flags = 128
visual = "moat"
)");
    write_text(directory.path() / "ground.toml", R"(
[[defs]]
id = "ground.grass"
name_key = "ground.grass.name"
move_cost = 1
flags = 0
visual = "ground/grass"
)");
    write_text(directory.path() / "site_projection.toml", R"(
[city_skeleton]
block_split_depth = 5
block_cut_min_percent = 36
block_cut_max_percent = 44
block_min_extent = 3
height_noise_amplitude = 96
max_buildable_slope = 8
water_inland_reach = 18

[[terrain_ground]]
terrain = "terrain.grassland"
ground = "ground.grass"
rough_ground = "ground.grass"
)");
    write_text(directory.path() / "site_city.toml", R"(
[fill]
base_density_percent = 20
development_density_per_level = 4
max_density_percent = 88

[fortification]
double_wall_defense = 80
tower_defense = 40
tower_spacing = 8
moat_defense = 60
breach_percent_at_full_damage = 12
wall_edge = "edge.city_wall"
gate_edge = "edge.city_gate"
tower_edge = "edge.city_wall_tower"
moat_edge = "edge.city_moat"

[[quotas]]
zone = "residential"
driver = "population"
units_per_block = 250
max_percent = 70

[[quotas]]
zone = "commercial"
driver = "development_level"
units_per_block = 2
max_percent = 25

[[building_defs]]
id = "building.cottage"
zone = "residential"
frontage = 2
depth = 2

[[building_defs]]
id = "building.shop"
zone = "commercial"
frontage = 2
depth = 2

[[building_defs]]
id = "building.market"
zone = "commercial"
frontage = 2
depth = 2
landmark = true

[[faction_styles]]
faction = 0
landmarks = ["building.market"]
)");
    write_text(directory.path() / "site_wild.toml", R"(
[wilderness]
height_noise_amplitude = 72
plain_passable_slope = 28
hills_passable_slope = 20
mountain_passable_slope = 9
jitter_cell_extent = 3
sparse_vegetation_percent = 12
forest_vegetation_percent = 70
base_resource_points = 3
mine_resource_points = 12
owned_encounter_points = 2
unowned_encounter_points = 6
road_traveler_points = 3
wilderness_portals = 1
mountain_portals = 2
ruin_portals = 4
ruin_keep_min_percent = 20
ruin_keep_max_percent = 40
)");
    std::filesystem::copy_file(std::filesystem::path{AETHERIA_SOURCE_DIR} / "data" /
                                   "site_build.toml",
                               directory.path() / "site_build.toml");
    return rules::RulesetLoader::load(directory.path());
}

}  // namespace aetheria::tests
