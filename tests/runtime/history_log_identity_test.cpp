#include "core/history/history_log.h"
#include "core/runtime/playable_session.h"
#include "core/runtime/save_raws.h"
#include "core/runtime/session_persistence.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/save_manifest_io.h"
#include "sim/world_hash.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>

#include "tests/runtime/history_log_test_support.h"

namespace {

using aetheria::runtime::PlayableBattleChoice;
using aetheria::runtime::PlayableSession;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;

using namespace aetheria::tests::history_log;



TEST(HistoryAcceptance, EveryIdentityComponentAndFoldOrderChangesComposition) {
    TemporaryDirectory workspace;
    const auto baseline_slot = workspace.path() / "baseline";
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(999999), 51, AETHERIA_SOURCE_DIR "/data", active};
        aetheria::zone::FileZoneStore destination{baseline_slot, test_ruleset()};
        session.save_game(destination, "default");
    }
    const auto baseline = aetheria::sim::world_state_hash(baseline_slot);

    const auto zone_slot = workspace.path() / "zone";
    std::filesystem::copy(baseline_slot, zone_slot,
                          std::filesystem::copy_options::recursive);
    std::filesystem::path changed_zone_path;
    std::string original_zone_bytes;
    {
        aetheria::zone::FileZoneStore store{zone_slot, test_ruleset()};
        const auto keys = store.stored_keys();
        const auto region = std::ranges::find_if(
            keys, [](const auto key) { return key != aetheria::zone::kRootZone; });
        ASSERT_NE(region, keys.end());
        auto changed = store.load(*region);
        ASSERT_NE(changed, nullptr);
        changed_zone_path = store.path_for(*region);
        original_zone_bytes = read_bytes(changed_zone_path);
        auto& tiles =
            std::get<aetheria::zone::RegionPayload>(changed->payload).layers.at(0);
        ++tiles.temperature.at(0);
        store.save(*changed);
    }
    const auto zone_changed = aetheria::sim::world_state_hash(zone_slot);
    EXPECT_NE(zone_changed.zone_hash, baseline.zone_hash);
    EXPECT_EQ(zone_changed.raws_hash, baseline.raws_hash);
    EXPECT_EQ(zone_changed.history_head_hash, baseline.history_head_hash);
    EXPECT_NE(zone_changed.hash, baseline.hash);
    write_bytes(changed_zone_path, original_zone_bytes);
    const auto zone_restored = aetheria::sim::world_state_hash(zone_slot);
    EXPECT_EQ(zone_restored.hash, baseline.hash);
    std::cout << "acceptance_components zone_injected=RED before=" << baseline.hash
              << " after=" << zone_changed.hash
              << " zone_restored=GREEN actual=" << zone_restored.hash << '\n';

    const auto raws_slot = workspace.path() / "raws";
    std::filesystem::copy(baseline_slot, raws_slot,
                          std::filesystem::copy_options::recursive);
    const auto manifest_path = raws_slot / "manifest.bin";
    const auto original_manifest_bytes = read_bytes(manifest_path);
    auto manifest = aetheria::zone::detail::decode_manifest(
        original_manifest_bytes);
    const auto terrain_path =
        aetheria::runtime::save_raws_directory(raws_slot) / "terrain.toml";
    const auto original_terrain = read_bytes(terrain_path);
    write_bytes(terrain_path, original_terrain +
                                  "\n# world identity negative control\n");
    manifest.raws_hash = aetheria::runtime::save_raws_hash(raws_slot);
    aetheria::zone::detail::atomic_replace(
        manifest_path, aetheria::zone::detail::encode_manifest(manifest));
    const auto raws_changed = aetheria::sim::world_state_hash(raws_slot);
    EXPECT_EQ(raws_changed.zone_hash, baseline.zone_hash);
    EXPECT_NE(raws_changed.raws_hash, baseline.raws_hash);
    EXPECT_EQ(raws_changed.history_head_hash, baseline.history_head_hash);
    EXPECT_NE(raws_changed.hash, baseline.hash);
    write_bytes(terrain_path, original_terrain);
    aetheria::zone::detail::atomic_replace(manifest_path,
                                            original_manifest_bytes);
    const auto raws_restored = aetheria::sim::world_state_hash(raws_slot);
    EXPECT_EQ(raws_restored.hash, baseline.hash);
    std::cout << "acceptance_components raws_injected=RED before=" << baseline.hash
              << " after=" << raws_changed.hash
              << " raws_restored=GREEN actual=" << raws_restored.hash << '\n';

    const auto history_slot = workspace.path() / "history";
    std::filesystem::copy(baseline_slot, history_slot,
                          std::filesystem::copy_options::recursive);
    const auto history_path = history_slot / "history.log";
    const auto original_history = read_bytes(history_path);
    aetheria::history::HistoryLog history{history_path};
    static_cast<void>(history.append(aetheria::time::Tick{1},
                                     "identity_negative_control", "value = 1\n"));
    const auto history_changed = aetheria::sim::world_state_hash(history_slot);
    EXPECT_EQ(history_changed.zone_hash, baseline.zone_hash);
    EXPECT_EQ(history_changed.raws_hash, baseline.raws_hash);
    EXPECT_NE(history_changed.history_head_hash, baseline.history_head_hash);
    EXPECT_NE(history_changed.hash, baseline.hash);
    write_bytes(history_path, original_history);
    const auto history_restored = aetheria::sim::world_state_hash(history_slot);
    EXPECT_EQ(history_restored.hash, baseline.hash);
    std::cout << "acceptance_components head_injected=RED before=" << baseline.hash
              << " after=" << history_changed.hash
              << " head_restored=GREEN actual=" << history_restored.hash << '\n';

    const auto reordered = fold_identity_for_test(
        baseline.raws_hash, baseline.zone_hash, baseline.history_head_hash);
    const auto canonical = fold_identity_for_test(
        baseline.zone_hash, baseline.raws_hash, baseline.history_head_hash);
    EXPECT_NE(reordered, baseline.hash);
    EXPECT_EQ(canonical, baseline.hash);
    std::cout << "acceptance_components order_swapped=RED canonical=" << baseline.hash
              << " swapped=" << reordered
              << " order_restored=GREEN actual=" << canonical << '\n';
}

}  // namespace
