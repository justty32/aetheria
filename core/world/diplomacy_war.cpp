// 外交條約、宣戰理由與持續戰爭推進。

#include "core/world/diplomacy.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace aetheria::world {
namespace {

[[nodiscard]] std::int32_t saturating_int32(std::int64_t value) noexcept {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        value, std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()));
}

} // namespace

const ActiveTreaty& WorldDiplomacyState::start_treaty(rules::TreatyDefId def,
                                                      FactionId first,
                                                      FactionId second,
                                                      time::Tick now) {
    static_cast<void>(faction_index(first));
    static_cast<void>(faction_index(second));
    const auto* definition = ruleset_->treaty(def);
    if (definition == nullptr || first == second) {
        throw std::invalid_argument{"條約定義或締約方無效"};
    }
    std::optional<time::Tick> expiry;
    if (definition->duration_xun != 0) {
        expiry = now + time::kXun *
                           static_cast<std::int64_t>(definition->duration_xun);
    }
    treaties_.push_back({.def = def,
                         .parties = {first, second},
                         .started = now,
                         .expires_at = expiry});
    return treaties_.back();
}

const CasusBelliClaim&
WorldDiplomacyState::grant_casus_belli(FactionId owner, FactionId target,
                                       rules::CasusBelliDefId def,
                                       time::Tick now) {
    static_cast<void>(faction_index(owner));
    static_cast<void>(faction_index(target));
    const auto* definition = ruleset_->casus_belli(def);
    if (definition == nullptr || owner == target ||
        definition->duration_xun == 0) {
        throw std::invalid_argument{"宣戰理由定義或對象無效"};
    }
    casus_belli_.push_back({
        .owner = owner,
        .target = target,
        .def = def,
        .granted_at = now,
        .expires_at = now + time::kXun * static_cast<std::int64_t>(
                                             definition->duration_xun),
    });
    return casus_belli_.back();
}

bool WorldDiplomacyState::has_casus_belli(FactionId owner, FactionId target,
                                          rules::CasusBelliDefId def,
                                          time::Tick now) const noexcept {
    return std::ranges::any_of(casus_belli_, [&](const CasusBelliClaim& claim) {
        return claim.owner == owner && claim.target == target &&
               claim.def == def && now < claim.expires_at;
    });
}

WarEvent&
WorldDiplomacyState::declare_war(FactionId attacker, FactionId defender,
                                 std::optional<rules::CasusBelliDefId> cause,
                                 time::Tick now,
                                 bool defensive_alliance_obligation) {
    static_cast<void>(faction_index(attacker));
    static_cast<void>(faction_index(defender));
    if (attacker == defender ||
        (cause.has_value() &&
         !has_casus_belli(attacker, defender, *cause, now))) {
        throw std::invalid_argument{"宣戰方、目標或宣戰理由無效／已過期"};
    }
    if (!cause.has_value() && !defensive_alliance_obligation) {
        for (std::size_t third = 1; third <= faction_count_; ++third) {
            const auto faction = static_cast<FactionId>(third);
            if (faction != attacker && faction != defender) {
                adjust_relation(faction, attacker,
                                {.trust = ruleset_->diplomacy_rules()
                                              .unjustified_war_trust_penalty});
            }
        }
    }
    wars_.push_back(
        {.participants = {attacker, defender}, .cause = cause, .started = now});
    return wars_.back();
}

void WorldDiplomacyState::advance_war_xun(
    WarEvent& war, std::array<std::uint32_t, 2> casualties) {
    if (!war.active) {
        throw std::invalid_argument{"已結束的戰爭不能推進"};
    }
    const auto& rules = ruleset_->diplomacy_rules().war_weariness;
    for (std::size_t side = 0; side < war.weariness.size(); ++side) {
        const auto casualty_weariness =
            (static_cast<std::int64_t>(casualties[side]) *
                 rules.per_thousand_casualties +
             999) /
            1000;
        war.weariness[side] =
            saturating_int32(static_cast<std::int64_t>(war.weariness[side]) +
                             rules.base_per_xun + casualty_weariness);
    }
}

void WorldDiplomacyState::add_war_score(WarEvent& war,
                                        std::int32_t score_delta) noexcept {
    war.war_score = saturating_int32(static_cast<std::int64_t>(war.war_score) +
                                     score_delta);
}

bool WorldDiplomacyState::peace_pressure_reached(
    const WarEvent& war, std::size_t participant) const {
    if (participant >= war.weariness.size()) {
        throw std::out_of_range{"戰爭參與方下標超出範圍"};
    }
    return war.weariness[participant] >=
           ruleset_->diplomacy_rules().war_weariness.peace_threshold;
}

std::int32_t WorldDiplomacyState::player_peace_leverage(
    const WarEvent& war, FactionId perspective,
    std::int32_t third_party_pressure) const {
    const auto first = perspective == war.participants[0];
    if (!first && perspective != war.participants[1]) {
        throw std::invalid_argument{"和談視角不是參戰方"};
    }
    const auto own = first ? 0U : 1U;
    const auto opponent = 1U - own;
    return diplomacy::calculate_peace_leverage(
        {.war_score = first ? war.war_score : -war.war_score,
         .own_weariness = war.weariness[own],
         .opponent_weariness = war.weariness[opponent],
         .third_party_pressure = third_party_pressure},
        ruleset_->diplomacy_rules().peace_weights);
}

diplomacy::PeaceTerms
WorldDiplomacyState::peace_terms(std::int32_t leverage) const noexcept {
    return diplomacy::derive_peace_terms(
        leverage, ruleset_->diplomacy_rules().peace_thresholds);
}

} // namespace aetheria::world
