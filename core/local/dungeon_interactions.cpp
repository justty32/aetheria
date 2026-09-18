// dungeon_interactions.cpp：機關、Boss、光源與持久寶藏互動。

#include "core/local/dungeon.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "core/rules/power.h"
#include "core/worldgen/region_seed.h"

#include "core/local/dungeon_detail.h"

namespace aetheria::local::detail::dungeon {

[[nodiscard]] bool contains_uid(const std::vector<std::uint64_t>& values,
                                std::uint64_t uid) noexcept {
    return std::ranges::binary_search(values, uid);
}

}  // namespace aetheria::local::detail::dungeon

namespace aetheria::local {
using detail::dungeon::contains_uid;

namespace {

void insert_uid(std::vector<std::uint64_t>& values, std::uint64_t uid) {
    const auto position = std::ranges::lower_bound(values, uid);
    if (position == values.end() || *position != uid) {
        values.insert(position, uid);
    }
}

[[nodiscard]] std::int32_t attribute_value(const rules::Attributes& attributes,
                                           rules::TrapCheckAttribute attribute) noexcept {
    switch (attribute) {
        case rules::TrapCheckAttribute::Skill:
            return attributes.skill;
        case rules::TrapCheckAttribute::Mind:
            return attributes.mind;
        case rules::TrapCheckAttribute::Spirit:
            return attributes.spirit;
    }
    return 0;
}

}  // namespace


bool valid_dungeon_persistent_state(const DungeonPersistentState& state) noexcept {
    const auto valid = [](const auto& values) {
        return std::ranges::is_sorted(values) &&
               std::ranges::adjacent_find(values) == values.end() &&
               std::ranges::none_of(values, [](std::uint64_t uid) { return uid == 0; });
    };
    return valid(state.triggered_trap_uids) && valid(state.claimed_treasure_uids);
}

rules::CheckResult detect_trap(const DungeonTrap& trap, std::uint8_t roll,
                               const rules::Attributes& attributes,
                               const rules::Ruleset& ruleset, bool has_light) {
    const auto* definition = ruleset.trap(trap.definition);
    if (definition == nullptr) {
        throw std::invalid_argument{"偵測機關收到無效 TrapDefId"};
    }
    return rules::evaluate_check(roll, attribute_value(attributes, definition->detection_attribute),
                                 dungeon_light_effect(has_light, ruleset).detection_modifier,
                                 definition->detection_difficulty + trap.depth * 2,
                                 ruleset.check_rules());
}

std::optional<rules::CheckResult> disarm_trap(const DungeonTrap& trap, std::uint8_t roll,
                                              const rules::Attributes& attributes,
                                              const rules::Ruleset& ruleset) {
    const auto* definition = ruleset.trap(trap.definition);
    if (definition == nullptr) {
        throw std::invalid_argument{"解除機關收到無效 TrapDefId"};
    }
    if (definition->disarm_method != rules::TrapDisarmMethod::Attribute) {
        return std::nullopt;
    }
    return rules::evaluate_check(roll, attribute_value(attributes, definition->disarm_attribute),
                                 0, definition->disarm_difficulty + trap.depth * 2,
                                 ruleset.check_rules());
}

TrapTriggerResult trigger_trap(const DungeonTrap& trap, std::uint64_t target_uid,
                               bool target_is_enemy, DungeonPersistentState& persistent,
                               const rules::Ruleset& ruleset) {
    const auto* definition = ruleset.trap(trap.definition);
    if (definition == nullptr || target_uid == 0 || !valid_dungeon_persistent_state(persistent)) {
        throw std::invalid_argument{"觸發機關收到無效 TrapDef、目標或持久層"};
    }
    const bool newly_triggered = !contains_uid(persistent.triggered_trap_uids, trap.uid);
    insert_uid(persistent.triggered_trap_uids, trap.uid);
    return {target_uid, target_is_enemy,
            static_cast<std::int64_t>(definition->base_damage) +
                static_cast<std::int64_t>(definition->damage_per_depth) * trap.depth,
            newly_triggered};
}

std::int64_t boss_damage(const DungeonBoss& boss, std::int64_t proposed_damage,
                         world::Significance attacker_tier, const rules::Ruleset& ruleset,
                         const rules::PowerBreakthroughDef* breakthrough) {
    if (!boss.alive) {
        throw std::invalid_argument{"不能攻擊已擊敗的地城 Boss"};
    }
    return rules::apply_individual_tier_gate(proposed_damage, attacker_tier, boss.combat_tier,
                                             true, ruleset.power_rules(), breakthrough);
}

DungeonBossDefeatEvent defeat_boss(DungeonBoss& boss, std::uint64_t dungeon_uid) {
    if (!boss.alive || dungeon_uid == 0) {
        throw std::invalid_argument{"地城 Boss 擊敗事件狀態無效"};
    }
    boss.alive = false;
    return {dungeon_uid, world::Significance::Region, "event.dungeon_boss_defeated"};
}

std::optional<std::uint8_t> light_exhaustion_depth(const DungeonGenerated& dungeon,
                                                   std::uint32_t light_supply) noexcept {
    std::uint64_t spent{};
    for (const auto& floor : dungeon.floors) {
        spent += floor.light_cost;
        if (spent > light_supply) {
            return floor.depth;
        }
    }
    return std::nullopt;
}

DungeonLightEffect dungeon_light_effect(bool has_light,
                                        const rules::Ruleset& ruleset) noexcept {
    const auto& rules = ruleset.dungeon_rules();
    if (has_light) {
        return {rules.lit_vision, 0, 0};
    }
    return {rules.unlit_vision, rules.unlit_hit_modifier, rules.unlit_detection_modifier};
}

std::uint64_t available_treasure_value(const DungeonGenerated& dungeon) noexcept {
    std::uint64_t result{};
    for (const auto& floor : dungeon.floors) {
        for (const auto& treasure : floor.treasures) {
            result += treasure.value;
        }
    }
    return result;
}

void claim_all_treasure(const DungeonGenerated& dungeon, DungeonPersistentState& persistent) {
    if (!valid_dungeon_persistent_state(persistent)) {
        throw std::invalid_argument{"領取寶藏收到無效持久層"};
    }
    for (const auto& floor : dungeon.floors) {
        for (const auto& treasure : floor.treasures) {
            insert_uid(persistent.claimed_treasure_uids, treasure.uid);
        }
    }
}

}  // namespace aetheria::local
