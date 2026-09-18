// core/rules/power_sources_magic.cpp：Region 儀式、Site 戰術與 Local 個體魔法。

#include "core/rules/power_sources.h"

#include <algorithm>
#include <stdexcept>

#include "core/rules/power_sources_detail.h"

namespace aetheria::rules {

using detail::checked_add;

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


}  // namespace aetheria::rules
