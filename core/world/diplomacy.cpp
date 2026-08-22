// 外交世界狀態的有向關係演化、期限檢查與持續戰爭推進。
// 關係矩陣只寫指定方向；所有可調速率與門檻均來自 Ruleset。

#include "core/world/diplomacy.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace aetheria::world {
namespace {

[[nodiscard]] constexpr std::size_t faction_value(FactionId faction) noexcept {
    return static_cast<std::size_t>(faction);
}

[[nodiscard]] std::int32_t
revert_toward_zero(std::int32_t value, std::uint16_t rate,
                   std::uint16_t denominator) noexcept {
    if (value == 0) {
        return 0;
    }
    const auto magnitude = std::abs(static_cast<std::int64_t>(value));
    const auto reduction = std::max<std::int64_t>(
        1, (magnitude * rate + denominator - 1) / denominator);
    return static_cast<std::int32_t>(value > 0 ? magnitude - reduction
                                               : reduction - magnitude);
}

[[nodiscard]] std::int32_t saturating_int32(std::int64_t value) noexcept {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        value, std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()));
}

} // namespace

WorldDiplomacyState::WorldDiplomacyState(std::uint16_t faction_count,
                                         std::uint64_t world_seed,
                                         const rules::Ruleset& ruleset)
    : faction_count_{faction_count}, world_seed_{world_seed},
      ruleset_{&ruleset} {
    if (faction_count == 0 || !ruleset.diplomacy_rules().loaded) {
        throw std::invalid_argument{
            "外交狀態需要至少一個勢力與已載入的外交規則"};
    }
    const auto extent = static_cast<std::size_t>(faction_count_) + 1U;
    relations_.resize(extent * extent);
    truths_.emplace(extent);
    knowledge_.emplace(extent * extent);
    faction_minds_.emplace(extent);
}

std::size_t WorldDiplomacyState::faction_index(FactionId faction) const {
    const auto index = faction_value(faction);
    if (index == 0 || index > faction_count_) {
        throw std::out_of_range{"外交勢力 id 超出範圍"};
    }
    return index;
}

std::size_t WorldDiplomacyState::matrix_index(FactionId observer,
                                              FactionId target) const {
    const auto extent = static_cast<std::size_t>(faction_count_) + 1U;
    return faction_index(observer) * extent + faction_index(target);
}

const DiplomaticRelation&
WorldDiplomacyState::relation(FactionId observer, FactionId target) const {
    return relations_.at(matrix_index(observer, target));
}

std::int32_t
WorldDiplomacyState::bounded_relation(std::int64_t value) const noexcept {
    const auto& rules = ruleset_->diplomacy_rules();
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        value, rules.relation_min, rules.relation_max));
}

void WorldDiplomacyState::set_relation(FactionId observer, FactionId target,
                                       DiplomaticRelation value) {
    const auto bounded = DiplomaticRelation{
        .favor = bounded_relation(value.favor),
        .trust = bounded_relation(value.trust),
        .fear = bounded_relation(value.fear),
        .grievance = bounded_relation(value.grievance),
    };
    relations_.at(matrix_index(observer, target)) = bounded;
}

void WorldDiplomacyState::adjust_relation(FactionId observer, FactionId target,
                                          DiplomaticRelation delta) {
    auto& value = relations_.at(matrix_index(observer, target));
    value.favor =
        bounded_relation(static_cast<std::int64_t>(value.favor) + delta.favor);
    value.trust =
        bounded_relation(static_cast<std::int64_t>(value.trust) + delta.trust);
    value.fear =
        bounded_relation(static_cast<std::int64_t>(value.fear) + delta.fear);
    value.grievance = bounded_relation(
        static_cast<std::int64_t>(value.grievance) + delta.grievance);
}

void WorldDiplomacyState::advance_relations_xun() {
    const auto& rates = ruleset_->diplomacy_rules().reversion;
    for (std::size_t observer = 1; observer <= faction_count_; ++observer) {
        for (std::size_t target = 1; target <= faction_count_; ++target) {
            auto& value =
                relations_[observer *
                               (static_cast<std::size_t>(faction_count_) + 1U) +
                           target];
            value.favor =
                revert_toward_zero(value.favor, rates.favor, rates.denominator);
            value.trust =
                revert_toward_zero(value.trust, rates.trust, rates.denominator);
            value.fear =
                revert_toward_zero(value.fear, rates.fear, rates.denominator);
            value.grievance = revert_toward_zero(
                value.grievance, rates.grievance, rates.denominator);
        }
    }
}

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

DiplomacyPersistentState WorldDiplomacyState::persistent_state() const {
    return {
        .faction_count = faction_count_,
        .world_seed = world_seed_,
        .relations = relations_,
        .treaties = treaties_,
        .casus_belli = casus_belli_,
        .wars = wars_,
        .faction_truths = truths_,
        .knowledge = knowledge_,
        .faction_minds = faction_minds_,
    };
}

WorldDiplomacyState
WorldDiplomacyState::restore(DiplomacyPersistentState state,
                             const rules::Ruleset& ruleset) {
    WorldDiplomacyState result{state.faction_count, state.world_seed, ruleset};
    const auto extent = static_cast<std::size_t>(state.faction_count) + 1U;
    if (state.relations.size() != extent * extent) {
        throw std::runtime_error{"外交存檔的關係矩陣尺寸不符"};
    }
    for (const auto& relation : state.relations) {
        for (const auto component : {relation.favor, relation.trust,
                                     relation.fear, relation.grievance}) {
            if (result.bounded_relation(component) != component) {
                throw std::runtime_error{"外交存檔的關係分量超出規則範圍"};
            }
        }
    }
    const auto require_faction = [&](FactionId faction) {
        static_cast<void>(result.faction_index(faction));
    };
    for (const auto& treaty : state.treaties) {
        require_faction(treaty.parties[0]);
        require_faction(treaty.parties[1]);
        if (treaty.parties[0] == treaty.parties[1] ||
            ruleset.treaty(treaty.def) == nullptr ||
            (treaty.expires_at.has_value() &&
             *treaty.expires_at < treaty.started)) {
            throw std::runtime_error{"外交存檔含無效條約實例"};
        }
    }
    for (const auto& claim : state.casus_belli) {
        require_faction(claim.owner);
        require_faction(claim.target);
        if (claim.owner == claim.target ||
            ruleset.casus_belli(claim.def) == nullptr ||
            claim.expires_at <= claim.granted_at) {
            throw std::runtime_error{"外交存檔含無效宣戰理由"};
        }
    }
    for (const auto& war : state.wars) {
        require_faction(war.participants[0]);
        require_faction(war.participants[1]);
        if (war.participants[0] == war.participants[1] ||
            (war.cause.has_value() &&
             ruleset.casus_belli(*war.cause) == nullptr) ||
            war.weariness[0] < 0 || war.weariness[1] < 0) {
            throw std::runtime_error{"外交存檔含無效戰爭事件"};
        }
    }
    if (state.faction_truths.has_value() && state.faction_truths->size() != extent) {
        throw std::runtime_error{"外交存檔的勢力真值尺寸不符"};
    }
    if (state.knowledge.has_value() && state.knowledge->size() != extent * extent) {
        throw std::runtime_error{"外交存檔的情報矩陣尺寸不符"};
    }
    if (state.faction_minds.has_value() && state.faction_minds->size() != extent) {
        throw std::runtime_error{"外交存檔的 AI 目標狀態尺寸不符"};
    }
    if (state.faction_truths.has_value()) {
        for (const auto& truth : *state.faction_truths) {
            if (truth.military_power < 0 || truth.economic_power < 0) {
                throw std::runtime_error{"外交存檔含負的勢力國力真值"};
            }
        }
    }
    if (state.knowledge.has_value()) {
        for (const auto& record : *state.knowledge) {
            if (record.military_power < 0 || record.economic_power < 0 ||
                record.uncertainty_permyriad > 10000 || record.distance < 0) {
                throw std::runtime_error{"外交存檔含無效情報快取"};
            }
        }
    }
    result.relations_ = std::move(state.relations);
    result.treaties_ = std::move(state.treaties);
    result.casus_belli_ = std::move(state.casus_belli);
    result.wars_ = std::move(state.wars);
    result.truths_ = std::move(state.faction_truths);
    result.knowledge_ = std::move(state.knowledge);
    result.faction_minds_ = std::move(state.faction_minds);
    return result;
}

} // namespace aetheria::world
