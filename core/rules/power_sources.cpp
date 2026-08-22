// core/rules/power_sources.cpp：三條位階來源的共用彙整與各自不可替代的規則。

#include "core/rules/power_sources.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace aetheria::rules {
namespace {

[[nodiscard]] std::uint8_t tier_value(world::Significance tier) {
    const auto value = static_cast<std::uint8_t>(tier);
    if (value > static_cast<std::uint8_t>(world::Significance::World)) {
        throw std::invalid_argument{"位階超出 Significance 範圍"};
    }
    return value;
}

[[nodiscard]] std::int32_t checked_add(std::int32_t left, std::int32_t right,
                                       std::string_view field) {
    const auto value = static_cast<std::int64_t>(left) + right;
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
        throw std::overflow_error{"位階來源屬性修正溢位：" + std::string{field}};
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] bool contains_ability(std::span<const std::string> abilities,
                                    std::string_view id) {
    return std::ranges::find(abilities, id) != abilities.end();
}

[[nodiscard]] std::uint32_t apply_faith_event(const FaithIntermediaryEvent& event,
                                              std::span<FaithFollower> followers,
                                              std::uint32_t& total_tiers_lost) {
    std::uint32_t affected{};
    for (auto& follower : followers) {
        const auto before = resolve_follower(follower).tier;
        bool matched{};
        for (auto& source : follower.sources) {
            if (source.definition != nullptr && source.available &&
                source.definition->kind == PowerSourceKind::Faith &&
                source.definition->origin_id == event.deity_id) {
                source.available = false;
                source.unavailable_reason = "deity_fallen";
                matched = true;
            }
        }
        if (!matched) {
            continue;
        }
        const auto after = resolve_follower(follower).tier;
        total_tiers_lost += tier_value(before) - tier_value(after);
        ++affected;
    }
    return affected;
}

}  // namespace

PowerProfile resolve_power_profile(world::Significance base_tier,
                                   world::Significance tier_cap,
                                   Attributes base_attributes,
                                   std::span<const PowerSourceState> sources) {
    auto tier = tier_value(base_tier);
    const auto cap = tier_value(tier_cap);
    PowerProfile result{.tier = base_tier, .attributes = base_attributes, .ability_ids = {}};
    for (const auto& state : sources) {
        if (!state.available) {
            continue;
        }
        if (state.definition == nullptr || state.definition->origin_id.empty() ||
            state.definition->removal_condition.empty() || state.definition->tier_bonus < 0) {
            throw std::invalid_argument{"啟用中的位階來源定義無效"};
        }
        const auto& source = *state.definition;
        tier = static_cast<std::uint8_t>(
            std::min<unsigned>(static_cast<unsigned>(world::Significance::World),
                               static_cast<unsigned>(tier) +
                                   static_cast<unsigned>(source.tier_bonus)));
        result.attributes.body =
            checked_add(result.attributes.body, source.attribute_modifier.body, "body");
        result.attributes.skill =
            checked_add(result.attributes.skill, source.attribute_modifier.skill, "skill");
        result.attributes.mind =
            checked_add(result.attributes.mind, source.attribute_modifier.mind, "mind");
        result.attributes.spirit =
            checked_add(result.attributes.spirit, source.attribute_modifier.spirit, "spirit");
        for (const auto& ability : source.ability_ids) {
            if (ability.empty()) {
                throw std::invalid_argument{"位階來源能力 id 不可為空"};
            }
            if (!contains_ability(result.ability_ids, ability)) {
                result.ability_ids.push_back(ability);
            }
        }
    }
    result.tier = static_cast<world::Significance>(std::min(tier, cap));
    return result;
}

bool begin_strategic_ritual(RegionMagicState& region, const StrategicSpellDef& spell,
                            std::int32_t available_casters,
                            const PowerSourceRules& rules) {
    if (!rules.loaded || spell.id.empty() || spell.scale != SpellScale::Region ||
        spell.magical_resource_cost < rules.strategic_minimum_resource_cost ||
        spell.ritual_duration_xun < rules.strategic_minimum_ritual_xun ||
        spell.required_casters < rules.strategic_minimum_casters ||
        spell.cooldown_xun < rules.strategic_minimum_cooldown_xun ||
        region.magical_resource < 0 || region.resource_income_per_xun < 0 ||
        region.cooldown_remaining_xun < 0 || available_casters < 0) {
        throw std::invalid_argument{"戰略魔法輸入未達昂貴且緩慢的不變式"};
    }
    if (region.cooldown_remaining_xun > 0 || !region.rituals.empty() ||
        available_casters < spell.required_casters ||
        region.magical_resource < spell.magical_resource_cost) {
        return false;
    }
    region.magical_resource -= spell.magical_resource_cost;
    region.rituals.push_back({
        .remaining_xun = spell.ritual_duration_xun,
        .cooldown_xun = spell.cooldown_xun,
        .weather_delta = spell.weather_delta,
        .combat_posture_delta = spell.combat_posture_delta,
    });
    return true;
}

void advance_region_magic(RegionMagicState& region, std::int32_t elapsed_xun) {
    if (elapsed_xun < 0 || region.magical_resource < 0 || region.resource_income_per_xun < 0 ||
        region.cooldown_remaining_xun < 0 || region.rituals.size() > 1U) {
        throw std::invalid_argument{"Region 魔法狀態無效"};
    }
    for (std::int32_t step = 0; step < elapsed_xun; ++step) {
        region.magical_resource = checked_add(region.magical_resource,
                                              region.resource_income_per_xun,
                                              "magical_resource");
        if (!region.rituals.empty()) {
            auto& ritual = region.rituals.front();
            if (ritual.remaining_xun <= 0 || ritual.cooldown_xun < 0) {
                throw std::invalid_argument{"戰略儀式進度無效"};
            }
            --ritual.remaining_xun;
            if (ritual.remaining_xun == 0) {
                region.weather_modifier = checked_add(
                    region.weather_modifier, ritual.weather_delta, "weather_modifier");
                region.combat_posture_modifier =
                    checked_add(region.combat_posture_modifier,
                                ritual.combat_posture_delta, "combat_posture_modifier");
                region.cooldown_remaining_xun = ritual.cooldown_xun;
                region.rituals.clear();
            }
        } else if (region.cooldown_remaining_xun > 0) {
            --region.cooldown_remaining_xun;
        }
    }
}

bool cast_tactical_spell(std::int32_t& mana, FormationMagicState& formation,
                         const TacticalSpellDef& spell) {
    if (spell.id.empty() || spell.scale != SpellScale::Site || spell.mana_cost <= 0 || mana < 0 ||
        spell.summoned_mass < 0) {
        throw std::invalid_argument{"戰術魔法輸入無效"};
    }
    if (mana < spell.mana_cost) {
        return false;
    }
    mana -= spell.mana_cost;
    formation.attack_modifier =
        checked_add(formation.attack_modifier, spell.attack_modifier, "formation_attack");
    formation.defense_modifier =
        checked_add(formation.defense_modifier, spell.defense_modifier, "formation_defense");
    formation.summoned_mass =
        checked_add(formation.summoned_mass, spell.summoned_mass, "summoned_mass");
    return true;
}

IndividualCastResult cast_individual_spell(IndividualCasterState& caster,
                                           const IndividualSpellDef& spell,
                                           std::uint8_t roll,
                                           const CheckRules& check_rules) {
    if (spell.id.empty() || spell.scale != SpellScale::Local || spell.mana_cost <= 0 ||
        spell.failure_backlash <= 0 || spell.effect_value < 0 || caster.mana < 0 ||
        caster.health < 0) {
        throw std::invalid_argument{"個體法術輸入無效或失敗沒有代價"};
    }
    if (caster.mana < spell.mana_cost) {
        throw std::invalid_argument{"施法者魔力不足"};
    }
    caster.mana -= spell.mana_cost;
    const auto check = evaluate_check(roll, caster.mind, spell.modifier,
                                      spell.difficulty, check_rules);
    IndividualCastResult result{.check = check, .applied_effect = 0, .backlash_damage = 0};
    if (check.success) {
        result.applied_effect = static_cast<std::int32_t>(
            static_cast<std::int64_t>(spell.effect_value) * check.effect_percent / 100);
    } else {
        result.backlash_damage = std::min(caster.health, spell.failure_backlash);
        caster.health -= result.backlash_damage;
    }
    return result;
}

std::uint32_t retry_success_permyriad(std::int32_t mind, std::int32_t modifier,
                                      std::int32_t difficulty,
                                      std::uint32_t attempts) noexcept {
    const auto target = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(mind) + modifier - difficulty, 0, 100);
    auto remaining_failure = UINT32_C(10'000);
    for (std::uint32_t attempt = 0; attempt < attempts; ++attempt) {
        remaining_failure = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(remaining_failure) * (100 - target) + 50U) / 100U);
    }
    return UINT32_C(10'000) - remaining_failure;
}

PowerProfile resolve_follower(const FaithFollower& follower) {
    if (follower.entity_id == 0) {
        throw std::invalid_argument{"信徒 entity id 不可為零"};
    }
    return resolve_power_profile(follower.base_tier, follower.tier_cap,
                                 follower.base_attributes, follower.sources);
}

std::uint32_t deprive_faith(FaithFollower& follower, std::string_view deity_id,
                            std::string reason) {
    if (deity_id.empty() || reason.empty()) {
        throw std::invalid_argument{"信仰剝奪必須具名神祇與理由"};
    }
    const auto before = resolve_follower(follower).tier;
    for (auto& source : follower.sources) {
        if (source.definition != nullptr && source.available &&
            source.definition->kind == PowerSourceKind::Faith &&
            source.definition->origin_id == deity_id) {
            source.available = false;
            source.unavailable_reason = reason;
        }
    }
    const auto after = resolve_follower(follower).tier;
    return tier_value(before) - tier_value(after);
}

DeityFallResult fall_deity(RootFaithState& root, std::string_view deity_id,
                           std::span<FaithFollower> followers) {
    if (deity_id.empty()) {
        throw std::invalid_argument{"隕落神祇 id 不可為空"};
    }
    auto found = std::ranges::find_if(root.deities, [&](const RootDeityState& deity) {
        return deity.definition != nullptr && deity.definition->id == deity_id;
    });
    if (found == root.deities.end() || found->definition->significance !=
                                          world::Significance::World) {
        throw std::invalid_argument{"神祇必須是 root 中的 World 級實體"};
    }
    if (!found->alive) {
        throw std::invalid_argument{"神祇已經隕落"};
    }
    found->alive = false;
    DeityFallResult result;
    result.event.deity_id = std::string{deity_id};
    for (const auto& follower : followers) {
        const bool follows = std::ranges::any_of(follower.sources, [&](const auto& source) {
            return source.definition != nullptr && source.available &&
                   source.definition->kind == PowerSourceKind::Faith &&
                   source.definition->origin_id == deity_id;
        });
        if (follows) {
            result.event.follower_ids.push_back(follower.entity_id);
        }
    }
    result.affected_followers =
        apply_faith_event(result.event, followers, result.total_tiers_lost);
    return result;
}

std::uint32_t count_tenet_conflicts(std::span<const std::string> selected_tenets,
                                    std::span<const TenetDef> definitions) {
    std::map<std::string_view, const TenetDef*, std::less<>> by_id;
    for (const auto& definition : definitions) {
        if (definition.id.empty() || !by_id.emplace(definition.id, &definition).second) {
            throw std::invalid_argument{"教義定義 id 為空或重複"};
        }
    }
    std::set<std::pair<std::string_view, std::string_view>> conflicts;
    for (const auto& selected : selected_tenets) {
        const auto found = by_id.find(selected);
        if (found == by_id.end()) {
            throw std::invalid_argument{"信徒引用不存在的教義：" + selected};
        }
        for (const auto& other : found->second->conflicts) {
            if (std::ranges::find(selected_tenets, other) != selected_tenets.end()) {
                conflicts.emplace(std::min<std::string_view>(selected, other),
                                  std::max<std::string_view>(selected, other));
            }
        }
    }
    return static_cast<std::uint32_t>(conflicts.size());
}

bool can_reach_within_lifespan(const RaceDef& race, world::Significance desired_tier,
                               std::uint32_t required_years) noexcept {
    return desired_tier <= race.tier_cap && required_years <= race.lifespan_years;
}

}  // namespace aetheria::rules
