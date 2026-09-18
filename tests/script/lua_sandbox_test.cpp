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

}  // namespace
