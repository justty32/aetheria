#include "core/script/script_engine.h"

#include "core/serialize/zone_codec.h"
#include "core/world/region_movement.h"
#include "tests/support/performance.h"
#include "tests/world/region_test_support.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "tests/script/lua_sandbox_test_support.h"

namespace {

using aetheria::script::HookKind;
using aetheria::script::Outcome;
using aetheria::script::ScriptEngine;
using aetheria::script::ScriptError;
using aetheria::script::ScriptSource;
using aetheria::script::ScriptWorldState;
using aetheria::script::TurnScriptPlan;
using aetheria::world::FactionId;
using aetheria::world::RegionTiles;
using aetheria::world::RegionTurnPipeline;
using aetheria::world::RegionXY;
using aetheria::world::TurnClock;
using aetheria::world::TurnStage;
using aetheria::zone::InMemoryZoneStore;

using namespace aetheria::tests::lua_sandbox_support;

TEST(LuaSandbox, MissingIdRejectsReloadAtomicallyWithoutTouchingWorld) {
    BoundWorld fixture{"reload.required"};
    ScriptEngine engine;
    ASSERT_TRUE(load(engine, fixture.world, "old.lua", R"lua(
return { id = "reload.required", effect = function(ctx) ctx:set_owner("capital", 8) end }
)lua"));
    engine.event_effect("reload.required", fixture.world, UINT64_C(8));
    const auto hash_before = fixture.world.deterministic_hash();
    const auto events_before = fixture.world.events();
    const auto ids_before = engine.definition_ids();

    const auto rejected = engine.replace_sources({ScriptSource{"missing.lua", R"lua(
return { id = "reload.other", effect = function(ctx) ctx:set_owner("capital", 9) end }
)lua"}},
                                                 fixture.world);
    EXPECT_FALSE(rejected.accepted);
    EXPECT_NE(rejected.error.find("reload.required"), std::string::npos);
    EXPECT_EQ(fixture.world.deterministic_hash(), hash_before);
    EXPECT_EQ(fixture.world.events(), events_before);
    EXPECT_EQ(engine.definition_ids(), ids_before);
    EXPECT_EQ(fixture.world.owner("capital"), 8);
    std::cout << "reload_rejected_error=" << rejected.error << " world_hash_before=" << hash_before
              << " after=" << fixture.world.deterministic_hash() << '\n';
}

TEST(LuaSandbox, DirectoryReloadRebuildsStateAndPerformanceUsesMinOfFive) {
    BoundWorld fixture{"reload.directory"};
    ScriptEngine engine;
    const auto directory = std::filesystem::temp_directory_path() / "aetheria-m8-lua-reload";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto script_path = directory / "event.lua";
    {
        std::ofstream stream{script_path};
        stream << R"lua(
reload_counter = (reload_counter or 0) + 1
return {
    id = "reload.directory",
    condition = function(ctx) return true end,
    effect = function(ctx) ctx:set_owner("capital", reload_counter) end,
    on_trigger = function(ctx) end,
    check = function(ctx) return outcome.ongoing end
}
)lua";
    }
    const auto reloaded = engine.reload_directory(directory, fixture.world);
    ASSERT_TRUE(reloaded.accepted) << reloaded.error;
    const TurnScriptPlan plan{{"reload.directory"}, {"reload.directory"}, {"reload.directory"}};
    using Clock = std::chrono::steady_clock;
    const auto minimum = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = Clock::now();
        aetheria::script::run_turn_scripts(engine, fixture.world, plan, UINT64_C(80));
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    });
    std::filesystem::remove_all(directory);

    EXPECT_EQ(fixture.world.owner("capital"), 1);
    EXPECT_EQ(engine.call_counts(), (aetheria::script::HookCallCounts{1, 1, 1, 1}));
    EXPECT_LT(engine.call_counts().total(), 10000U);
    EXPECT_LT(minimum, 250.0);
    std::cout << "script_turn_min_of_" << aetheria::tests::kPerformanceSampleCount
              << "_ms=" << minimum << " total_calls=" << engine.call_counts().total() << '\n';
}

}  // namespace
