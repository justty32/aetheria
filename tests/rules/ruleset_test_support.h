#pragma once

// tests/rules 底下 Ruleset 載入／驗證測試共用的暫存目錄與最小合法 ruleset fixture。

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
        path_ = std::filesystem::temp_directory_path() /
                ("aetheria-ruleset-" + std::to_string(stamp) + "-" +
                 std::to_string(sequence.fetch_add(1)));
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
)toml");
}

}  // namespace aetheria::tests
