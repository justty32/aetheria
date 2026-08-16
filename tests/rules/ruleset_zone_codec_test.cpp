#include "core/rules/ruleset.h"
#include "core/serialize/zone_codec.h"
#include "core/zone/zone_key.h"
#include "tests/rules/ruleset_test_support.h"

#include <iostream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::RulesetLoader;
using aetheria::rules::value_of;
using aetheria::serialize::decode_zone;
using aetheria::serialize::encode_zone;
using aetheria::tests::kGrass;
using aetheria::tests::kOcean;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::write_valid_ruleset;
using aetheria::zone::child_key;
using aetheria::zone::kRootZone;
using aetheria::zone::Zone;

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
