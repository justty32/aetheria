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

struct BoundWorld {
    explicit BoundWorld(std::string definition_id)
        : definition_id{std::move(definition_id)}, tiles{1, 1} {
        world.bind_region_tile("capital", this->definition_id, tiles, RegionXY{0, 0});
    }

    std::string definition_id;
    RegionTiles tiles;
    ScriptWorldState world;
};

[[nodiscard]] bool load(ScriptEngine& engine, ScriptWorldState& world, std::string origin,
                        std::string text) {
    const auto result =
        engine.replace_sources({ScriptSource{std::move(origin), std::move(text)}}, world);
    EXPECT_TRUE(result.accepted) << result.error;
    return result.accepted;
}

[[nodiscard]] std::string joined(const std::vector<std::string>& values) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) {
            result += ',';
        }
        result += value;
    }
    return result;
}

TEST(LuaDeterminismLaw1, SandboxEnvironmentHasOnlyTheCompleteAllowlist) {
    BoundWorld fixture{"law1"};
    ScriptEngine engine;
    ASSERT_TRUE(load(engine, fixture.world, "law1.lua", R"lua(
return {
    id = "law1",
    condition = function(ctx)
        return ctx ~= nil and os == nil and io == nil and collectgarbage == nil
            and math.random == nil and math.randomseed == nil
    end
}
)lua"));

    const std::vector<std::string> expected{
        "_ENV",    "_VERSION", "assert", "error", "ipairs",   "math",     "next", "ordered_pairs",
        "outcome", "select",   "string", "table", "tonumber", "tostring", "type", "utf8",
    };
    const auto keys = engine.sandbox_keys();
    std::cout << "sandbox_env_keys=" << joined(keys) << '\n';
    EXPECT_EQ(keys, expected);
    EXPECT_TRUE(engine.event_condition("law1", fixture.world, UINT64_C(1)));
}

TEST(LuaDeterminismLaw2, InjectedRngIsTheOnlySourceAndMatchesCoreMt19937_64) {
    BoundWorld fixture{"law2"};
    ScriptEngine engine;
    ASSERT_TRUE(load(engine, fixture.world, "law2.lua", R"lua(
return {
    id = "law2",
    condition = function(ctx)
        return ctx.rng ~= nil and math.random == nil and random_source == nil
    end,
    effect = function(ctx)
        ctx:set_owner("capital", ctx.rng:int(1, 65535))
    end
}
)lua"));

    constexpr auto world_seed = UINT64_C(0x8A37E21A);
    EXPECT_TRUE(engine.event_condition("law2", fixture.world, world_seed));
    engine.event_effect("law2", fixture.world, world_seed);
    std::mt19937_64 reference{
        aetheria::script::derive_script_seed(world_seed, "law2", HookKind::EventEffect)};
    const auto expected = std::uniform_int_distribution<std::int64_t>{1, 65535}(reference);
    EXPECT_EQ(fixture.world.owner("capital"), expected);
    EXPECT_EQ(fixture.world.events().size(), 1U);
    std::cout << "injected_rng_expected=" << expected
              << " actual=" << fixture.world.owner("capital") << '\n';
}

TEST(LuaDeterminismLaw3, OrderedPairsSortsKeysAndLintRejectsRawPairs) {
    BoundWorld fixture{"law3"};
    ScriptEngine engine;
    ASSERT_TRUE(load(engine, fixture.world, "law3.lua", R"lua(
return {
    id = "law3",
    effect = function(ctx)
        local result = 0
        for key, value in ordered_pairs({ z = 3, a = 1, m = 2 }) do
            result = result * 10 + value
        end
        ctx:set_owner("capital", result)
    end
}
)lua"));
    engine.event_effect("law3", fixture.world, UINT64_C(3));
    EXPECT_EQ(fixture.world.owner("capital"), 123);

    const auto rejected = engine.replace_sources({ScriptSource{"raw-pairs.lua", R"lua(
return { id = "law3", effect = function(ctx)
    for key, value in pairs({ a = 1 }) do ctx:set_owner(key, value) end
end }
)lua"}},
                                                 fixture.world);
    EXPECT_FALSE(rejected.accepted);
    EXPECT_NE(rejected.error.find("lint 禁止使用 pairs"), std::string::npos);
}

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

TEST(LuaSandbox, SameSeedAndScriptProduceTheSameWorldHash) {
    BoundWorld first{"determinism"};
    BoundWorld second{"determinism"};
    ScriptEngine first_engine;
    ScriptEngine second_engine;
    const std::string source = R"lua(
return {
    id = "determinism",
    effect = function(ctx)
        local value = 0
        for key, digit in ordered_pairs({ z = 3, a = 1, m = 2 }) do
            value = value * 10 + digit
        end
        ctx:set_owner("capital", value + ctx.rng:int(1, 100))
    end
}
)lua";
    ASSERT_TRUE(load(first_engine, first.world, "first.lua", source));
    ASSERT_TRUE(load(second_engine, second.world, "second.lua", source));
    first_engine.event_effect("determinism", first.world, UINT64_C(0xD37E2));
    second_engine.event_effect("determinism", second.world, UINT64_C(0xD37E2));
    EXPECT_EQ(first.world.deterministic_hash(), second.world.deterministic_hash());
    EXPECT_EQ(first.world.owner("capital"), second.world.owner("capital"));
    std::cout << "script_world_hash_first=" << first.world.deterministic_hash()
              << " second=" << second.world.deterministic_hash() << '\n';
}

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
