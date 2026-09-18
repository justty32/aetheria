// 外交持久狀態快照與還原驗證。

#include "core/world/diplomacy.h"

#include <stdexcept>
#include <utility>

namespace aetheria::world {

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
