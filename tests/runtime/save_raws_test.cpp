#include "tests/runtime/save_raws_test_support.h"

namespace {

using namespace aetheria::tests::save_raws;

TEST(SaveRaws, OldWorldIsolatedFromGlobalDataAndTamperingRejected) {
    TemporaryDirectory workspace;
    const auto private_data_slot = workspace.path() / "private-data";
    const auto private_data = aetheria::runtime::save_raws_directory(private_data_slot);
    static_cast<void>(aetheria::runtime::copy_save_raws(
        std::filesystem::path{AETHERIA_SOURCE_DIR} / "data", private_data_slot));
    const auto old_slot = workspace.path() / "old-world";
    {
        auto ruleset = std::make_unique<aetheria::rules::Ruleset>(
            aetheria::rules::RulesetLoader::load(private_data));
        aetheria::zone::InMemoryZoneStore active_store{*ruleset};
        PlayableSession session{UINT64_C(515151), 51, private_data.string(), active_store};
        aetheria::zone::FileZoneStore destination{old_slot, *ruleset};
        session.save_game(destination, "default");
    }

    const auto old_raws = aetheria::runtime::save_raws_directory(old_slot);
    ASSERT_EQ(toml_count(old_raws), 22U);
    for (const auto& entry : std::filesystem::directory_iterator{private_data}) {
        if (entry.is_regular_file() && entry.path().extension() == ".toml") {
            EXPECT_EQ(read_file(entry.path()), read_file(old_raws / entry.path().filename()));
        }
    }
    const auto copied_hash = aetheria::runtime::save_raws_hash(old_slot);
    {
        LoadedWorld loaded{old_slot};
        ASSERT_TRUE(loaded.store->manifest().has_value());
        EXPECT_EQ(loaded.store->manifest()->raws_hash, copied_hash);
    }

    const auto before = observe(old_slot);
    auto terrain = read_file(private_data / "terrain.toml");
    terrain += R"(

[[defs]]
id = "terrain.moon_moss"
name_key = "terrain.moon_moss.name"
move_cost = 4
flags = 1
visual = "terrain/moon_moss"
yield = { food = 0, production = 0, wealth = 0, mana = 3 }
)";
    write_file(private_data / "terrain.toml", terrain);
    const auto after_new_terrain = observe(old_slot);
    EXPECT_EQ(after_new_terrain, before);
    std::cout << "global_change=add_terrain before_hash=" << before.world_hash
              << " after_hash=" << after_new_terrain.world_hash
              << " before_terrain_count=" << before.terrain_count
              << " after_terrain_count=" << after_new_terrain.terrain_count << '\n';

    replace_once(terrain,
                 "id = \"terrain.swamp\"\nname_key = \"terrain.swamp.name\"\nmove_cost = 3",
                 "id = \"terrain.swamp\"\nname_key = \"terrain.swamp.name\"\nmove_cost = 9");
    write_file(private_data / "terrain.toml", terrain);
    const auto after_move_cost = observe(old_slot);
    EXPECT_EQ(after_move_cost, before);
    std::cout << "global_change=swamp_move_cost before_hash=" << before.world_hash
              << " after_hash=" << after_move_cost.world_hash
              << " before_move_cost=" << before.swamp_move_cost
              << " after_move_cost=" << after_move_cost.swamp_move_cost << '\n';

    auto civilization = read_file(private_data / "civilization.toml");
    replace_once(civilization, "faction_count = 3", "faction_count = 4");
    write_file(private_data / "civilization.toml", civilization);
    const auto after_faction_count = observe(old_slot);
    EXPECT_EQ(after_faction_count, before);
    std::cout << "global_change=faction_count before_hash=" << before.world_hash
              << " after_hash=" << after_faction_count.world_hash
              << " before_factions=" << before.faction_count
              << " after_factions=" << after_faction_count.faction_count << '\n';

    const auto original_old_terrain = read_file(old_raws / "terrain.toml");
    write_file(old_raws / "terrain.toml", original_old_terrain + " ");
    try {
        static_cast<void>(aetheria::sim::world_state_hash(old_slot));
        FAIL() << "tampered raws should have been rejected";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("raws 內容雜湊不符"), std::string::npos);
        std::cout << "raws_tampered rejected=1 error=" << error.what() << '\n';
    }
    write_file(old_raws / "terrain.toml", original_old_terrain);
    const auto restored_hash = aetheria::sim::world_state_hash(old_slot).hash;
    EXPECT_EQ(restored_hash, before.world_hash);
    std::cout << "raws_restored accepted=1 world_hash=" << restored_hash << '\n';

    const auto missing_raws_slot = workspace.path() / "missing-raws";
    std::filesystem::copy(old_slot, missing_raws_slot,
                          std::filesystem::copy_options::recursive);
    std::filesystem::remove_all(aetheria::runtime::save_raws_directory(missing_raws_slot));
    try {
        static_cast<void>(aetheria::sim::world_state_hash(missing_raws_slot));
        FAIL() << "missing raws should have been rejected";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("存檔 raws"), std::string::npos);
    }

    civilization += R"(

[[faction_defs]]
faction = 4
id = "faction.moon_moss"
expansion = 40
aggression = 20
fidelity = 70
commerce = 30
piety = 80
caution = 60
resentment = 10
)";
    write_file(private_data / "civilization.toml", civilization);
    auto projection = read_file(private_data / "site_projection.toml");
    projection += R"(

[[terrain_ground]]
terrain = "terrain.moon_moss"
ground = "ground.mud"
rough_ground = "ground.stone"
)";
    write_file(private_data / "site_projection.toml", projection);
    const auto new_slot = workspace.path() / "new-world";
    {
        auto ruleset = std::make_unique<aetheria::rules::Ruleset>(
            aetheria::rules::RulesetLoader::load(private_data));
        ASSERT_TRUE(ruleset->find_terrain("terrain.moon_moss").has_value());
        EXPECT_EQ(ruleset->civilization_rules().factions.faction_count, 4U);
        aetheria::zone::InMemoryZoneStore active_store{*ruleset};
        PlayableSession session{UINT64_C(515151), 51, private_data.string(), active_store};
        aetheria::zone::FileZoneStore destination{new_slot, *ruleset};
        session.save_game(destination, "default");
    }
    const auto new_world = observe(new_slot);
    const auto old_world = observe(old_slot);
    EXPECT_EQ(old_world, before);
    EXPECT_EQ(new_world.terrain_count, before.terrain_count + 1U);
    EXPECT_EQ(new_world.swamp_move_cost, 9);
    EXPECT_EQ(new_world.faction_count, 4U);
    EXPECT_EQ(new_world.diplomacy_faction_count, 4U);
    std::cout << "coexisting_worlds old_hash=" << old_world.world_hash
              << " old_terrain_count=" << old_world.terrain_count
              << " old_move_cost=" << old_world.swamp_move_cost
              << " old_factions=" << old_world.faction_count
              << " new_hash=" << new_world.world_hash
              << " new_terrain_count=" << new_world.terrain_count
              << " new_move_cost=" << new_world.swamp_move_cost
              << " new_factions=" << new_world.faction_count << '\n';
}

}  // namespace
