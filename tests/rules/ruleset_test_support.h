#pragma once

// tests/rules 底下 Ruleset 載入／驗證測試共用的暫存目錄與最小合法 ruleset
// fixture。

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace aetheria::tests {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        static std::atomic_uint64_t sequence{};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ =
            std::filesystem::temp_directory_path() / ("aetheria-ruleset-" + std::to_string(stamp) +
                                                      "-" + std::to_string(sequence.fetch_add(1)));
        if (!std::filesystem::create_directories(path_)) {
            throw std::runtime_error{"無法建立 Ruleset 測試目錄"};
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  private:
    std::filesystem::path path_;
};

inline void write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream stream{path};
    stream << text;
    if (!stream) {
        throw std::runtime_error{"無法寫 Ruleset 測試檔"};
    }
}

constexpr std::string_view kGrass = R"toml([[defs]]
id="terrain.grassland"
name_key="grass"
move_cost=1
flags=0
visual="grass"
yield={food=1,production=1,wealth=0,mana=0}
)toml";

constexpr std::string_view kOcean = R"toml([[defs]]
id="terrain.ocean"
name_key="ocean"
move_cost=2
flags=0
visual="ocean"
yield={food=1,production=0,wealth=1,mana=0}
)toml";

constexpr std::string_view kSiteSkeletonRules = R"toml([city_skeleton]
block_split_depth=5
block_cut_min_percent=36
block_cut_max_percent=44
block_min_extent=3
height_noise_amplitude=96
max_buildable_slope=8
water_inland_reach=18
)toml";

constexpr std::string_view kSiteFillRules = R"toml([fill]
base_density_percent=20
development_density_per_level=4
max_density_percent=88
[fortification]
double_wall_defense=80
tower_defense=40
tower_spacing=8
moat_defense=60
breach_percent_at_full_damage=12
wall_edge="edge.city_wall"
gate_edge="edge.city_gate"
tower_edge="edge.city_wall_tower"
moat_edge="edge.city_moat"
[[quotas]]
zone="residential"
driver="population"
units_per_block=250
max_percent=70
[[quotas]]
zone="commercial"
driver="development_level"
units_per_block=2
max_percent=25
[[building_defs]]
id="building.cottage"
zone="residential"
frontage=2
depth=2
[[building_defs]]
id="building.shop"
zone="commercial"
frontage=2
depth=2
[[building_defs]]
id="building.market"
zone="commercial"
frontage=2
depth=2
landmark=true
[[faction_styles]]
faction=0
landmarks=["building.market"]
)toml";

constexpr std::string_view kSiteWildRules = R"toml([wilderness]
height_noise_amplitude=72
plain_passable_slope=28
hills_passable_slope=20
mountain_passable_slope=9
jitter_cell_extent=3
sparse_vegetation_percent=12
forest_vegetation_percent=70
base_resource_points=3
mine_resource_points=12
owned_encounter_points=2
unowned_encounter_points=6
road_traveler_points=3
wilderness_portals=1
mountain_portals=2
ruin_portals=4
ruin_keep_min_percent=20
ruin_keep_max_percent=40
)toml";

inline void write_valid_ruleset(const std::filesystem::path& path, std::string_view terrain,
                                std::string_view feature_reference = {}) {
    write_text(path / "terrain.toml", terrain);
    write_text(path / "relief.toml", R"toml([[defs]]
id="relief.plain"
name_key="plain"
move_cost=1
flags=0
visual="plain"
)toml");
    std::string feature = R"toml([[defs]]
id="feature.none"
name_key="none"
move_cost=1
flags=0
visual="none"
)toml";
    if (!feature_reference.empty()) {
        feature += "required_terrain=\"" + std::string{feature_reference} + "\"\n";
    }
    write_text(path / "feature.toml", feature);
    write_text(path / "edges.toml", R"toml([[defs]]
id="edge.none"
name_key="none"
move_cost=1
flags=0
visual="none"
[[defs]]
id="edge.city_wall"
name_key="wall"
move_cost=1000
flags=8
visual="wall"
[[defs]]
id="edge.city_gate"
name_key="gate"
move_cost=1
flags=57
visual="gate"
[[defs]]
id="edge.city_wall_tower"
name_key="tower"
move_cost=1000
flags=72
visual="tower"
[[defs]]
id="edge.city_moat"
name_key="moat"
move_cost=1000
flags=128
visual="moat"
)toml");
    write_text(path / "ground.toml", R"toml([[defs]]
id="ground.grass"
name_key="grass"
move_cost=1
flags=0
visual="grass"
[[defs]]
id="ground.water"
name_key="water"
move_cost=0
flags=1
visual="water"
)toml");
    std::string projection{kSiteSkeletonRules};
    if (terrain.find("terrain.grassland") != std::string_view::npos) {
        projection += R"toml([[terrain_ground]]
terrain="terrain.grassland"
ground="ground.grass"
rough_ground="ground.grass"
)toml";
    }
    if (terrain.find("terrain.ocean") != std::string_view::npos) {
        projection += R"toml([[terrain_ground]]
terrain="terrain.ocean"
ground="ground.water"
rough_ground="ground.water"
)toml";
    }
    write_text(path / "site_projection.toml", projection);
    write_text(path / "site_city.toml", kSiteFillRules);
    write_text(path / "site_wild.toml", kSiteWildRules);
}

}  // namespace aetheria::tests
