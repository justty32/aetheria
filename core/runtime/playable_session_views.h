#pragma once

// 可玩情境的部隊、格網與回合批次檢視型別。

#include "core/rules/combat.h"
#include "core/rules/ruleset.h"
#include "core/runtime/character_save.h"
#include "core/narrative/emergent_quest.h"
#include "core/site/site_projection.h"
#include "core/time/tick.h"
#include "core/world/combat_scaling.h"
#include "core/world/army_state.h"
#include "core/world/diplomacy.h"
#include "core/world/named_fate.h"
#include "core/world/region_movement.h"
#include "core/world/region_tiles.h"
#include "core/zone/zone_store.h"
#include "core/zone/zone_manager.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aetheria::runtime {

struct PlayableArmy {
    world::StableId id;
    entt::entity entity{entt::null};
};

struct PlayableArmyView {
    world::StableId id;
    world::FactionId faction{};
    world::RegionXY tile;
    std::int32_t power{};
    bool player_controlled{};
};

struct PlayableTileState {
    world::FactionId owner{};
    world::PopulationReduction::Value population{};
    world::OrderReduction::Value order{};
};

struct PlayableAdvanceReport {
    std::vector<world::TurnStage> stages;
    std::vector<std::uint8_t> ai_actions;
    bool encounter_pending{};
};

// 三層地圖只把顯示用批次格碼交給 bridge；玩法真值仍留在各 Zone。
struct PlayableGridView {
    std::uint32_t width{};
    std::uint32_t height{};
    std::int8_t z{};
    std::vector<std::uint8_t> cells;
    std::uint16_t player_x{};
    std::uint16_t player_y{};
};

} // namespace aetheria::runtime
