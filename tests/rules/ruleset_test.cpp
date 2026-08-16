#include "core/rules/ruleset.h"
#include "core/serialize/zone_codec.h"
#include "core/zone/zone_key.h"
#include "tests/support/ruleset_fixture.h"

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::rules::RulesetLoader;
using aetheria::rules::TerrainDef;
using aetheria::rules::value_of;
using aetheria::serialize::decode_zone;
using aetheria::serialize::encode_zone;
using aetheria::tests::test_ruleset;
using aetheria::zone::child_key;
using aetheria::zone::kRootZone;
using aetheria::zone::Zone;

static_assert(std::same_as<decltype(std::declval<const Ruleset&>().terrains()),
                           std::span<const TerrainDef>>);
static_assert(!std::is_assignable_v<
              decltype((std::declval<const Ruleset&>().terrains()[0].move_cost)), std::int32_t>);

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

void write_text(const std::filesystem::path& path, std::string_view text) {
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

void write_valid_ruleset(const std::filesystem::path& path, std::string_view terrain,
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

TEST(RulesetLoader, LoadsFourImmutableDefinitionTypes) {
    const auto& ruleset = test_ruleset();
    ASSERT_EQ(ruleset.terrains().size(), 4U);
    ASSERT_EQ(ruleset.reliefs().size(), 3U);
    ASSERT_EQ(ruleset.features().size(), 5U);
    ASSERT_EQ(ruleset.edges().size(), 6U);
    ASSERT_EQ(ruleset.biome_rules().size(), 5U);
    EXPECT_TRUE(ruleset.biome_rules().back().fallback);
    EXPECT_TRUE(ruleset.movement_rules().loaded);
    EXPECT_EQ(ruleset.terrain(*ruleset.find_terrain("terrain.grassland"))->move_cost, 1);
    EXPECT_EQ(ruleset.feature(*ruleset.find_feature("feature.forest"))->required_terrain,
              ruleset.find_terrain("terrain.grassland"));
}

TEST(RulesetLoader, RejectsMissingFile) {
    TemporaryDirectory directory;
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsMalformedToml) {
    TemporaryDirectory directory;
    write_text(directory.path() / "terrain.toml", "[[defs]\nid='broken'");
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsMissingDefsSection) {
    TemporaryDirectory directory;
    write_text(directory.path() / "terrain.toml", "title='missing defs'\n");
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsGloballyDuplicateId) {
    TemporaryDirectory directory;
    write_valid_ruleset(directory.path(), std::string{kGrass} + std::string{kGrass});
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsIdWithoutTypePrefix) {
    TemporaryDirectory directory;
    auto terrain = std::string{kGrass};
    const auto position = terrain.find("terrain.grassland");
    ASSERT_NE(position, std::string::npos);
    terrain.replace(position, std::string{"terrain.grassland"}.size(), "grassland");
    write_valid_ruleset(directory.path(), terrain);
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsMoveCostBelowOne) {
    TemporaryDirectory directory;
    auto terrain = std::string{kGrass};
    const auto position = terrain.find("move_cost=1");
    ASSERT_NE(position, std::string::npos);
    terrain.replace(position, std::string{"move_cost=1"}.size(), "move_cost=0");
    write_valid_ruleset(directory.path(), terrain);
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsIntegerNarrowingOutsideInt32) {
    TemporaryDirectory directory;
    auto terrain = std::string{kGrass};
    const auto position = terrain.find("food=1");
    ASSERT_NE(position, std::string::npos);
    terrain.replace(position, std::string{"food=1"}.size(), "food=2147483648");
    write_valid_ruleset(directory.path(), terrain);
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsUnresolvedDefinitionReference) {
    TemporaryDirectory directory;
    write_valid_ruleset(directory.path(), kGrass, "terrain.missing");
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(ZoneCodec, RemapsSavedTerrainIndicesAfterTomlDefinitionOrderChanges) {
    TemporaryDirectory old_directory;
    TemporaryDirectory reordered_directory;
    write_valid_ruleset(old_directory.path(), std::string{kGrass} + std::string{kOcean});
    write_valid_ruleset(reordered_directory.path(), std::string{kOcean} + std::string{kGrass});
    const auto old_ruleset = RulesetLoader::load(old_directory.path());
    const auto reordered_ruleset = RulesetLoader::load(reordered_directory.path());
    const auto old_grass = *old_ruleset.find_terrain("terrain.grassland");
    const auto new_grass = *reordered_ruleset.find_terrain("terrain.grassland");
    ASSERT_NE(value_of(old_grass), value_of(new_grass));
    std::cout << "terrain.grassland old_index=" << value_of(old_grass)
              << " reordered_index=" << value_of(new_grass) << '\n';

    Zone source{child_key(kRootZone, 1, 0)};
    auto& source_tiles =
        std::get<aetheria::zone::RegionPayload>(source.payload)
            .layers.emplace(0, aetheria::world::RegionTiles{2, 1})
            .first->second;
    source_tiles.base = {old_grass, *old_ruleset.find_terrain("terrain.ocean")};
    const auto saved = encode_zone(source, old_ruleset);
    const auto loaded = decode_zone(saved, reordered_ruleset);

    const auto& loaded_tiles =
        std::get<aetheria::zone::RegionPayload>(loaded->payload).layers.at(0);
    EXPECT_EQ(loaded_tiles.base.at(0), new_grass);
    EXPECT_EQ(reordered_ruleset.terrain(loaded_tiles.base.at(0))->id,
              "terrain.grassland");
    EXPECT_EQ(reordered_ruleset.terrain(loaded_tiles.base.at(1))->id, "terrain.ocean");
    EXPECT_EQ(encode_zone(*decode_zone(encode_zone(*loaded, reordered_ruleset), reordered_ruleset),
                          reordered_ruleset),
              encode_zone(*loaded, reordered_ruleset));
}

TEST(ZoneCodec, RejectsSavedStringIdMissingFromCurrentRuleset) {
    TemporaryDirectory old_directory;
    TemporaryDirectory missing_directory;
    write_valid_ruleset(old_directory.path(), std::string{kGrass} + std::string{kOcean});
    write_valid_ruleset(missing_directory.path(), kOcean);
    const auto old_ruleset = RulesetLoader::load(old_directory.path());
    const auto missing_ruleset = RulesetLoader::load(missing_directory.path());
    Zone source{child_key(kRootZone, 1, 0)};
    auto& source_tiles =
        std::get<aetheria::zone::RegionPayload>(source.payload)
            .layers.emplace(0, aetheria::world::RegionTiles{1, 1})
            .first->second;
    source_tiles.base.at(0) = *old_ruleset.find_terrain("terrain.grassland");
    const auto saved = encode_zone(source, old_ruleset);

    try {
        static_cast<void>(decode_zone(saved, missing_ruleset));
        FAIL() << "dangling string id should throw";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("terrain.grassland"), std::string::npos);
    }
}

}  // namespace
