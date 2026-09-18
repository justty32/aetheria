#include "core/serialize/all_components.h"
#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/registry_codec.h"
#include "core/serialize/zone_codec_detail.h"
#include "core/world/diplomacy.h"
#include "core/world/faction_ai.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/save_manifest_io.h"
#include "sim/world_hash.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "tests/zone/diplomacy_save_test_support.h"

namespace {

using aetheria::rules::Ruleset;
using aetheria::serialize::AllComponentsV15;
using aetheria::serialize::normalized_state_hash;
using aetheria::serialize::RegistryOutputArchive;
using aetheria::serialize::save_registry_snapshot;
using aetheria::serialize::ZoneDecodeMode;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::time::kXun;
using aetheria::time::Tick;
using aetheria::world::DiplomaticRelation;
using aetheria::world::FactionId;
using aetheria::world::WorldDiplomacyState;
using aetheria::zone::FileZoneStore;
using aetheria::zone::kRootZone;
using aetheria::zone::SaveManifest;
using aetheria::zone::Zone;

using namespace aetheria::tests::diplomacy_save;

[[nodiscard]] std::string encode_v14_root_zone(const Ruleset& ruleset) {
    const Zone root{kRootZone};
    std::ostringstream stream{std::ios::binary};
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        constexpr std::uint32_t version = 14;
        const auto key = aetheria::zone::value_of(root.key);
        const auto tick = static_cast<std::int64_t>(root.last_saved_tick);
        auto terrains = terrain_ids(ruleset);
        auto reliefs = definition_ids(ruleset.reliefs());
        auto features = definition_ids(ruleset.features());
        auto edges = definition_ids(ruleset.edges());
        constexpr std::uint8_t payload_index = 0;
        archive(aetheria::serialize::detail::kZoneMagic, version, key, tick,
                aetheria::serialize::detail::kReservedPersistenceFlags, terrains, reliefs, features,
                edges, payload_index);
    }
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        RegistryOutputArchive registry_archive{archive};
        save_registry_snapshot(root.reg, registry_archive, AllComponentsV15{});
    }
    return std::move(stream).str();
}

TEST(DiplomacySave, V14RootLoadsWithDiplomacyAbsentRatherThanNeutral) {
    const auto loaded = aetheria::serialize::decode_zone(
        encode_v14_root_zone(test_ruleset()), test_ruleset(), ZoneDecodeMode::LegacyFixture);
    ASSERT_NE(loaded, nullptr);
    EXPECT_FALSE(loaded->diplomacy.has_value());

    Zone neutral{kRootZone};
    neutral.diplomacy.emplace(3, 0, test_ruleset());
    const auto absent_hash = normalized_state_hash(*loaded, test_ruleset());
    const auto neutral_hash = normalized_state_hash(neutral, test_ruleset());
    EXPECT_NE(absent_hash, neutral_hash);
    std::cout << "diplomacy_v14 presence=0 absent_hash=" << absent_hash
              << " neutral_presence=1 neutral_hash=" << neutral_hash << '\n';
}

}  // namespace
