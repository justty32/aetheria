// 外交持久狀態的現行位元流：v15 欄位後追加真值、情報快取與 AI 目標。

#include "core/serialize/zone_diplomacy_codec.h"

#include "core/serialize/zone_codec.h"

#include <cereal/types/string.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>


namespace aetheria::serialize::detail {

namespace {

[[nodiscard]] std::uint16_t faction_value(world::FactionId faction) noexcept {
    return static_cast<std::uint16_t>(faction);
}

[[nodiscard]] std::int64_t tick_value(time::Tick tick) noexcept {
    return static_cast<std::int64_t>(tick);
}

template <typename Def>
[[nodiscard]] const std::string& require_definition_id(const Def* definition, const char* kind) {
    if (definition == nullptr) {
        throw std::runtime_error{std::string{"外交存檔含無效"} + kind + "定義"};
    }
    return definition->id;
}

} // namespace

void save_diplomacy(cereal::PortableBinaryOutputArchive& archive,
                    const std::optional<world::WorldDiplomacyState>& diplomacy,
                    const rules::Ruleset& ruleset) {
    const bool present = diplomacy.has_value();
    archive(present);
    if (!present) {
        return;
    }
    const auto state = diplomacy->persistent_state();
    if (state.faction_count != ruleset.civilization_rules().factions.faction_count) {
        throw std::runtime_error{"外交狀態的勢力數與目前 Ruleset 不符"};
    }
    static_cast<void>(world::WorldDiplomacyState::restore(state, ruleset));
    archive(state.faction_count, state.world_seed);

    const auto relation_count = static_cast<std::uint64_t>(state.relations.size());
    archive(relation_count);
    for (const auto& relation : state.relations) {
        archive(relation.favor, relation.trust, relation.fear, relation.grievance);
    }

    const auto treaty_count = static_cast<std::uint64_t>(state.treaties.size());
    archive(treaty_count);
    for (const auto& treaty : state.treaties) {
        const auto& id = require_definition_id(ruleset.treaty(treaty.def), "條約");
        const auto first = faction_value(treaty.parties[0]);
        const auto second = faction_value(treaty.parties[1]);
        const auto started = tick_value(treaty.started);
        const bool has_expiry = treaty.expires_at.has_value();
        const auto expiry = has_expiry ? tick_value(*treaty.expires_at) : INT64_C(0);
        archive(id, first, second, started, has_expiry, expiry);
    }

    const auto claim_count = static_cast<std::uint64_t>(state.casus_belli.size());
    archive(claim_count);
    for (const auto& claim : state.casus_belli) {
        const auto& id = require_definition_id(ruleset.casus_belli(claim.def), "宣戰理由");
        const auto owner = faction_value(claim.owner);
        const auto target = faction_value(claim.target);
        const auto granted = tick_value(claim.granted_at);
        const auto expires = tick_value(claim.expires_at);
        archive(owner, target, id, granted, expires);
    }

    const auto war_count = static_cast<std::uint64_t>(state.wars.size());
    archive(war_count);
    for (const auto& war : state.wars) {
        const auto first = faction_value(war.participants[0]);
        const auto second = faction_value(war.participants[1]);
        const bool has_cause = war.cause.has_value();
        std::string cause_id;
        if (has_cause) {
            cause_id = require_definition_id(ruleset.casus_belli(*war.cause), "宣戰理由");
        }
        const auto started = tick_value(war.started);
        archive(first, second, has_cause, cause_id, started, war.war_score, war.weariness[0],
                war.weariness[1], war.active);
    }

    const bool truths_present = state.faction_truths.has_value();
    archive(truths_present);
    if (truths_present) {
        archive(static_cast<std::uint64_t>(state.faction_truths->size()));
        for (const auto& truth : *state.faction_truths) {
            archive(truth.military_power, truth.economic_power, truth.present);
        }
    }
    const bool knowledge_present = state.knowledge.has_value();
    archive(knowledge_present);
    if (knowledge_present) {
        archive(static_cast<std::uint64_t>(state.knowledge->size()));
        for (const auto& record : *state.knowledge) {
            const auto observed_at = tick_value(record.observed_at);
            archive(record.military_power, record.economic_power,
                    record.uncertainty_permyriad, observed_at, record.distance,
                    record.observed);
        }
    }
    const bool minds_present = state.faction_minds.has_value();
    archive(minds_present);
    if (minds_present) {
        archive(static_cast<std::uint64_t>(state.faction_minds->size()));
        for (const auto& mind : *state.faction_minds) {
            const auto goal = static_cast<std::uint8_t>(mind.goal);
            const bool has_forced_goal = mind.forced_goal.has_value();
            const auto forced_goal = static_cast<std::uint8_t>(
                has_forced_goal ? *mind.forced_goal : ai::FactionGoal::Prosper);
            archive(goal, mind.goal_score, mind.goal_switches, mind.initialized,
                    has_forced_goal, forced_goal);
        }
    }
}

} // namespace aetheria::serialize::detail
