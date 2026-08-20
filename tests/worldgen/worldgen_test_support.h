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
[[terrain_ground]]
terrain = "terrain.grassland"
ground = "ground.grass"
rough_ground = "ground.grass"
)");
    return rules::RulesetLoader::load(directory.path());
}

}  // namespace aetheria::tests
