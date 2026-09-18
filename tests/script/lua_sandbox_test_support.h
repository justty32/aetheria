#pragma once

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

namespace aetheria::tests::lua_sandbox_support {

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

[[nodiscard]] inline bool load(ScriptEngine& engine, ScriptWorldState& world, std::string origin,
                        std::string text) {
    const auto result =
        engine.replace_sources({ScriptSource{std::move(origin), std::move(text)}}, world);
    EXPECT_TRUE(result.accepted) << result.error;
    return result.accepted;
}

} // namespace aetheria::tests::lua_sandbox_support
