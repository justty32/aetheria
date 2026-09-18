#pragma once

#include "core/world/diplomacy.h"
#include "tests/support/ruleset_fixture.h"

#include <aetheria/ai/faction_view.h>

#include <array>
#include <concepts>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include <gtest/gtest.h>

namespace aetheria::tests::diplomacy {

using aetheria::ai::FactionView;
using aetheria::rules::Ruleset;
using aetheria::tests::test_ruleset;
using aetheria::time::kXun;
using aetheria::time::Tick;
using aetheria::world::DiplomaticRelation;
using aetheria::world::FactionId;
using aetheria::world::WorldDiplomacyState;

[[nodiscard]] inline WorldDiplomacyState world(std::uint64_t seed = 7) {
    return WorldDiplomacyState{3, seed, test_ruleset()};
}

}  // namespace aetheria::tests::diplomacy
