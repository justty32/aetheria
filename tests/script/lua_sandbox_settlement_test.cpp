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

TEST(LuaDeterminismLaw4, FreshEnvironmentDropsCrossTurnStateAndSaveOmitsLuaState) {
    auto zone = aetheria::tests::movement_zone();
    auto& tiles = std::get<aetheria::zone::RegionPayload>(zone->payload).layers.at(0);
    ScriptWorldState world;
    world.bind_region_tile("capital", "law4", tiles, RegionXY{0, 0});
    ScriptEngine engine;
    ASSERT_TRUE(load(engine, world, "law4.lua", R"lua(
persistent_counter = (persistent_counter or 0) + 1
lua_state_token = "LUA_STATE_SENTINEL_M8"
return {
    id = "law4",
    effect = function(ctx) ctx:set_owner("capital", persistent_counter) end
}
)lua"));

    engine.event_effect("law4", world, UINT64_C(4));
    engine.event_effect("law4", world, UINT64_C(4));
    EXPECT_EQ(world.owner("capital"), 1);
    EXPECT_EQ(world.events().size(), 1U);

    const auto bytes = aetheria::serialize::encode_zone(*zone, aetheria::tests::test_ruleset());
    EXPECT_EQ(bytes.find("LUA_STATE_SENTINEL_M8"), std::string::npos);
    std::cout << "save_bytes=" << bytes.size()
              << " lua_state_sentinel_offset=" << bytes.find("LUA_STATE_SENTINEL_M8") << '\n';
}

TEST(LuaDeterminismLaw5, PipelineRunsSynchronousHooksOnlyAtTheEventsStage) {
    auto zone = aetheria::tests::movement_zone();
    auto& tiles = std::get<aetheria::zone::RegionPayload>(zone->payload).layers.at(0);
    ScriptWorldState world;
    world.bind_region_tile("capital", "law5", tiles, RegionXY{0, 0});
    ScriptEngine engine;
    ASSERT_TRUE(load(engine, world, "law5.lua", R"lua(
return {
    id = "law5",
    condition = function(ctx) return coroutine == nil and async == nil end,
    effect = function(ctx) ctx:set_owner("capital", 5) end,
    on_trigger = function(ctx) ctx:set_owner("capital", 6) end,
    check = function(ctx) return outcome.ongoing end
}
)lua"));

    InMemoryZoneStore store{aetheria::tests::test_ruleset()};
    RegionTurnPipeline pipeline{aetheria::tests::test_ruleset(), store};
    TurnStage current_stage{};
    std::vector<TurnStage> callback_stages;
    const TurnScriptPlan plan{{"law5"}, {"law5"}, {"law5"}};
    pipeline.advance_xun(
        *zone, [&](TurnStage stage) { current_stage = stage; }, {}, {},
        [&](aetheria::time::Tick tick) {
            callback_stages.push_back(current_stage);
            aetheria::script::run_turn_scripts(engine, world, plan,
                                               static_cast<std::uint64_t>(tick));
        });

    EXPECT_EQ(callback_stages, std::vector{TurnStage::Events});
    EXPECT_EQ(engine.call_counts(), (aetheria::script::HookCallCounts{1, 1, 1, 1}));
    EXPECT_EQ(world.owner("capital"), 6);
    std::cout << "script_calls_per_turn condition=" << engine.call_counts().event_condition
              << " effect=" << engine.call_counts().event_effect
              << " narrative=" << engine.call_counts().narrative_trigger
              << " victory=" << engine.call_counts().victory_check << '\n';
}

TEST(LuaDeterminismLaw6, ErrorAbortsSettlementReportsItAndDoesNotCommit) {
    BoundWorld fixture{"law6"};
    ScriptEngine engine;
    ASSERT_TRUE(load(engine, fixture.world, "law6.lua", R"lua(
return {
    id = "law6",
    effect = function(ctx)
        ctx:set_owner("capital", 66)
        error("law6 deliberate failure")
    end
}
)lua"));

    try {
        engine.event_effect("law6", fixture.world, UINT64_C(6));
        FAIL() << "腳本錯誤被靜默吞掉";
    } catch (const ScriptError& error) {
        const std::string message = error.what();
        std::cout << "script_error_report=" << message << '\n';
        EXPECT_NE(message.find("law6.lua"), std::string::npos);
        EXPECT_NE(message.find("law6 deliberate failure"), std::string::npos);
    }
    EXPECT_EQ(fixture.world.owner("capital"), 0);
    EXPECT_TRUE(fixture.world.events().empty());
}

}  // namespace
