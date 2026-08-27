#include "core/runtime/playable_session.h"
#include "core/zone/file_zone_store.h"
#include "sim/world_hash.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace {

using aetheria::runtime::PlayableSession;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;

struct PlayableWorldSnapshot {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<aetheria::rules::TerrainId> base;
    std::vector<aetheria::rules::ReliefId> relief;
    std::vector<aetheria::rules::FeatureId> feature;
    std::vector<std::uint8_t> temperature;
    std::vector<std::uint8_t> moisture;
    std::vector<std::uint16_t> elevation;
    std::vector<aetheria::rules::EdgeId> edges;
    std::vector<aetheria::world::FactionId> owner;
    std::vector<aetheria::world::SettlementTier> settlement;
    std::vector<aetheria::world::PopulationReduction::Value> population;
    std::vector<aetheria::world::OrderReduction::Value> order;
    std::vector<std::tuple<std::uint64_t, std::uint16_t, std::int16_t,
                           std::int16_t, std::int32_t, bool>> armies;
    aetheria::world::DiplomacyPersistentState diplomacy;
    aetheria::time::Tick now{};

    bool operator==(const PlayableWorldSnapshot&) const = default;
};

[[nodiscard]] PlayableWorldSnapshot snapshot(const PlayableSession& session) {
    const auto& tiles = session.tiles();
    PlayableWorldSnapshot result;
    result.width = tiles.width;
    result.height = tiles.height;
    result.base = tiles.base;
    result.relief = tiles.relief;
    result.feature = tiles.feature;
    result.temperature = tiles.temperature;
    result.moisture = tiles.moisture;
    result.elevation = tiles.elevation;
    result.edges = tiles.edges;
    result.owner = tiles.owner;
    result.settlement = tiles.settlement;
    const auto population =
        tiles.reduction_values<aetheria::world::PopulationReduction>();
    result.population.assign(population.begin(), population.end());
    const auto order = tiles.reduction_values<aetheria::world::OrderReduction>();
    result.order.assign(order.begin(), order.end());
    for (const auto& army : session.armies()) {
        result.armies.emplace_back(
            army.id.uid, static_cast<std::uint16_t>(army.faction), army.tile.x,
            army.tile.y, army.power, army.player_controlled);
    }
    result.diplomacy = session.diplomacy().persistent_state();
    result.now = session.now();
    return result;
}

TEST(SessionPersistence, ColdFileStoreLoadPreservesPlayableWorldSnapshotAndHash) {
    TemporaryDirectory directory;
    std::optional<PlayableWorldSnapshot> before;
    std::uint64_t hash_before{};
    {
        aetheria::zone::InMemoryZoneStore active_store{test_ruleset()};
        auto session = std::make_unique<PlayableSession>(
            UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data", active_store);
        session->issue_move(session->player_army_id(), session->coverage_tile());
        for (std::size_t xun = 0; xun < 3; ++xun) {
            const auto report = session->advance_xun();
            ASSERT_FALSE(report.encounter_pending);
        }
        before = snapshot(*session);
        {
            aetheria::zone::FileZoneStore destination{directory.path(),
                                                      test_ruleset()};
            session->save_game(destination);
        }
        hash_before =
            aetheria::sim::world_state_hash(directory.path()).hash;
        session.reset();
    }

    aetheria::zone::FileZoneStore cold_store{directory.path(), test_ruleset()};
    auto loaded = PlayableSession::load(directory.path(), cold_store);
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(snapshot(*loaded), *before);
    loaded->save_game(cold_store);
    const auto hash_after =
        aetheria::sim::world_state_hash(directory.path()).hash;
    EXPECT_EQ(hash_after, hash_before);
}

TEST(SessionPersistence, RejectsSaveWhileEncounterIsPending) {
    TemporaryDirectory directory;
    aetheria::zone::InMemoryZoneStore active_store{test_ruleset()};
    PlayableSession session{UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data",
                            active_store};
    session.issue_move(session.player_army_id(), session.guided_target());
    static_cast<void>(session.advance_xun());
    const auto report = session.advance_xun();
    ASSERT_TRUE(report.encounter_pending);

    aetheria::zone::FileZoneStore destination{directory.path(), test_ruleset()};
    try {
        session.save_game(destination);
        FAIL() << "pending encounter save should have been rejected";
    } catch (const std::logic_error& error) {
        const std::string message{error.what()};
        EXPECT_NE(message.find("遭遇尚未處理"), std::string::npos);
        EXPECT_NE(message.find("回合尾端"), std::string::npos);
    }
    EXPECT_FALSE(destination.manifest().has_value());
}

}  // namespace
