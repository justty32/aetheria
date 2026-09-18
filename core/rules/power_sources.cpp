// core/rules/power_sources.cpp：三條位階來源的共用彙整與種族上限。

#include "core/rules/power_sources.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "core/rules/power_sources_detail.h"

namespace aetheria::rules {
namespace detail {

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

}  // namespace detail

namespace {

[[nodiscard]] bool contains_ability(std::span<const std::string> abilities,
                                    std::string_view id) {
    return std::ranges::find(abilities, id) != abilities.end();
}

}  // namespace

using detail::checked_add;
using detail::tier_value;

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

bool can_reach_within_lifespan(const RaceDef& race, world::Significance desired_tier,
                               std::uint32_t required_years) noexcept {
    return desired_tier <= race.tier_cap && required_years <= race.lifespan_years;
}


}  // namespace aetheria::rules
