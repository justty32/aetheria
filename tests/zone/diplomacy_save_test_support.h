#pragma once

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

namespace aetheria::tests::diplomacy_save {

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

[[nodiscard]] inline std::vector<std::string> terrain_ids(const Ruleset& ruleset) {
    std::vector<std::string> result;
    result.reserve(ruleset.terrains().size());
    for (const auto& definition : ruleset.terrains()) {
        result.push_back(definition.id);
    }
    return result;
}

template <typename Def>
[[nodiscard]] inline std::vector<std::string> definition_ids(std::span<const Def> definitions) {
    std::vector<std::string> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions) {
        result.push_back(definition.id);
    }
    return result;
}

inline void populate_non_default_diplomacy(WorldDiplomacyState& state) {
    state.set_faction_truth(FactionId{1}, 1357, 2468);
    state.set_faction_truth(FactionId{2}, 3579, 4680);
    state.set_faction_truth(FactionId{3}, 5791, 6802);
    state.observe_faction(FactionId{1}, FactionId{2}, 3200,
                          Tick{3 * static_cast<std::int64_t>(kXun)}, 11);
    state.observe_faction(FactionId{1}, FactionId{3}, 4700,
                          Tick{4 * static_cast<std::int64_t>(kXun)}, 19);
    auto& mind = state.faction_mind(FactionId{1});
    mind.goal = aetheria::ai::FactionGoal::Conquer;
    mind.goal_score = 777;
    mind.goal_switches = 9;
    mind.initialized = true;
    mind.forced_goal = aetheria::ai::FactionGoal::Conquer;
    state.set_relation(FactionId{1}, FactionId{2},
                       {.favor = 1234, .trust = -2345, .fear = 3456, .grievance = 4567});
    state.set_relation(FactionId{2}, FactionId{1},
                       {.favor = -765, .trust = 876, .fear = -987, .grievance = 1098});
    state.set_relation(FactionId{1}, FactionId{3},
                       {.favor = 111, .trust = -222, .fear = 333, .grievance = -444});

    const auto truce = *test_ruleset().find_treaty("treaty.truce");
    const auto alliance = *test_ruleset().find_treaty("treaty.defensive_alliance");
    static_cast<void>(state.start_treaty(truce, FactionId{1}, FactionId{2},
                                         Tick{2 * static_cast<std::int64_t>(kXun)}));
    static_cast<void>(state.start_treaty(alliance, FactionId{2}, FactionId{3},
                                         Tick{5 * static_cast<std::int64_t>(kXun)}));

    const auto revenge = *test_ruleset().find_casus_belli("casus_belli.revenge");
    static_cast<void>(state.grant_casus_belli(FactionId{1}, FactionId{2}, revenge,
                                              Tick{7 * static_cast<std::int64_t>(kXun)}));
    auto& war = state.declare_war(FactionId{1}, FactionId{2}, revenge,
                                  Tick{8 * static_cast<std::int64_t>(kXun)});
    state.advance_war_xun(war, {2000, 1000});
    state.add_war_score(war, 375);
}

}  // namespace aetheria::tests::diplomacy_save
