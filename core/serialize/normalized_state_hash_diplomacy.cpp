#include "core/serialize/normalized_state_hash_detail.h"

#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace aetheria::serialize::detail {

namespace {

[[nodiscard]] std::string_view treaty_id(const rules::Ruleset& ruleset,
                                         rules::TreatyDefId id) {
    const auto* definition = ruleset.treaty(id);
    if (definition == nullptr) {
        throw std::runtime_error{"正規化雜湊遇到無效條約 id"};
    }
    return definition->id;
}

[[nodiscard]] std::string_view casus_belli_id(
    const rules::Ruleset& ruleset, rules::CasusBelliDefId id) {
    const auto* definition = ruleset.casus_belli(id);
    if (definition == nullptr) {
        throw std::runtime_error{"正規化雜湊遇到無效宣戰理由 id"};
    }
    return definition->id;
}

[[nodiscard]] constexpr std::uint16_t
faction_value(world::FactionId faction) noexcept {
    return static_cast<std::uint16_t>(faction);
}

[[nodiscard]] constexpr std::int64_t tick_value(time::Tick tick) noexcept {
    return static_cast<std::int64_t>(tick);
}

} // namespace

void hash_diplomacy_state(std::uint64_t& hash, const zone::Zone& zone,
                          const rules::Ruleset& ruleset) {
    hash_scalar(hash, static_cast<std::uint8_t>(zone.diplomacy.has_value()));
    if (!zone.diplomacy.has_value()) {
        return;
    }
    if (zone.key != zone::kRootZone) {
        throw std::runtime_error{"正規化雜湊遇到非 root 的外交狀態"};
    }
    auto state = zone.diplomacy->persistent_state();
    const auto extent = static_cast<std::size_t>(state.faction_count) + 1U;
    if (state.relations.size() != extent * extent) {
        throw std::runtime_error{"正規化雜湊遇到無效外交關係矩陣"};
    }
    hash_scalar(hash, state.faction_count);
    hash_scalar(hash, state.world_seed);
    hash_scalar(hash, static_cast<std::uint64_t>(state.relations.size()));
    for (const auto& relation : state.relations) {
        hash_scalar(hash, relation.favor);
        hash_scalar(hash, relation.trust);
        hash_scalar(hash, relation.fear);
        hash_scalar(hash, relation.grievance);
    }

    std::ranges::sort(state.treaties, [&](const auto& left, const auto& right) {
        const auto left_expiry = left.expires_at.has_value()
                                     ? tick_value(*left.expires_at)
                                     : INT64_C(0);
        const auto right_expiry = right.expires_at.has_value()
                                      ? tick_value(*right.expires_at)
                                      : INT64_C(0);
        return std::tuple{treaty_id(ruleset, left.def),
                          faction_value(left.parties[0]),
                          faction_value(left.parties[1]), tick_value(left.started),
                          left.expires_at.has_value(), left_expiry} <
               std::tuple{treaty_id(ruleset, right.def),
                          faction_value(right.parties[0]),
                          faction_value(right.parties[1]), tick_value(right.started),
                          right.expires_at.has_value(), right_expiry};
    });
    hash_scalar(hash, static_cast<std::uint64_t>(state.treaties.size()));
    for (const auto& treaty : state.treaties) {
        hash_string(hash, treaty_id(ruleset, treaty.def));
        hash_scalar(hash, faction_value(treaty.parties[0]));
        hash_scalar(hash, faction_value(treaty.parties[1]));
        hash_scalar(hash, tick_value(treaty.started));
        hash_scalar(hash, static_cast<std::uint8_t>(treaty.expires_at.has_value()));
        if (treaty.expires_at.has_value()) {
            hash_scalar(hash, tick_value(*treaty.expires_at));
        }
    }

    std::ranges::sort(state.casus_belli, [&](const auto& left, const auto& right) {
        return std::tuple{casus_belli_id(ruleset, left.def),
                          faction_value(left.owner), faction_value(left.target),
                          tick_value(left.granted_at), tick_value(left.expires_at)} <
               std::tuple{casus_belli_id(ruleset, right.def),
                          faction_value(right.owner), faction_value(right.target),
                          tick_value(right.granted_at), tick_value(right.expires_at)};
    });
    hash_scalar(hash, static_cast<std::uint64_t>(state.casus_belli.size()));
    for (const auto& claim : state.casus_belli) {
        hash_string(hash, casus_belli_id(ruleset, claim.def));
        hash_scalar(hash, faction_value(claim.owner));
        hash_scalar(hash, faction_value(claim.target));
        hash_scalar(hash, tick_value(claim.granted_at));
        hash_scalar(hash, tick_value(claim.expires_at));
    }

    const auto cause_id = [&](const world::WarEvent& war) {
        return war.cause.has_value() ? casus_belli_id(ruleset, *war.cause)
                                     : std::string_view{};
    };
    std::ranges::sort(state.wars, [&](const auto& left, const auto& right) {
        return std::tuple{faction_value(left.participants[0]),
                          faction_value(left.participants[1]), cause_id(left),
                          tick_value(left.started), left.war_score, left.weariness[0],
                          left.weariness[1], left.active} <
               std::tuple{faction_value(right.participants[0]),
                          faction_value(right.participants[1]), cause_id(right),
                          tick_value(right.started), right.war_score,
                          right.weariness[0], right.weariness[1], right.active};
    });
    hash_scalar(hash, static_cast<std::uint64_t>(state.wars.size()));
    for (const auto& war : state.wars) {
        hash_scalar(hash, faction_value(war.participants[0]));
        hash_scalar(hash, faction_value(war.participants[1]));
        hash_scalar(hash, static_cast<std::uint8_t>(war.cause.has_value()));
        if (war.cause.has_value()) {
            hash_string(hash, cause_id(war));
        }
        hash_scalar(hash, tick_value(war.started));
        hash_scalar(hash, war.war_score);
        hash_scalar(hash, war.weariness[0]);
        hash_scalar(hash, war.weariness[1]);
        hash_scalar(hash, static_cast<std::uint8_t>(war.active));
    }

    hash_scalar(hash, static_cast<std::uint8_t>(state.faction_truths.has_value()));
    if (state.faction_truths.has_value()) {
        hash_scalar(hash, static_cast<std::uint64_t>(state.faction_truths->size()));
        for (const auto& truth : *state.faction_truths) {
            hash_scalar(hash, truth.military_power);
            hash_scalar(hash, truth.economic_power);
            hash_scalar(hash, static_cast<std::uint8_t>(truth.present));
        }
    }
    hash_scalar(hash, static_cast<std::uint8_t>(state.knowledge.has_value()));
    if (state.knowledge.has_value()) {
        hash_scalar(hash, static_cast<std::uint64_t>(state.knowledge->size()));
        for (const auto& record : *state.knowledge) {
            hash_scalar(hash, record.military_power);
            hash_scalar(hash, record.economic_power);
            hash_scalar(hash, record.uncertainty_permyriad);
            hash_scalar(hash, tick_value(record.observed_at));
            hash_scalar(hash, record.distance);
            hash_scalar(hash, static_cast<std::uint8_t>(record.observed));
        }
    }
    hash_scalar(hash, static_cast<std::uint8_t>(state.faction_minds.has_value()));
    if (state.faction_minds.has_value()) {
        hash_scalar(hash, static_cast<std::uint64_t>(state.faction_minds->size()));
        for (const auto& mind : *state.faction_minds) {
            hash_scalar(hash, static_cast<std::uint8_t>(mind.goal));
            hash_scalar(hash, mind.goal_score);
            hash_scalar(hash, mind.goal_switches);
            hash_scalar(hash, static_cast<std::uint8_t>(mind.initialized));
            hash_scalar(hash, static_cast<std::uint8_t>(mind.forced_goal.has_value()));
            if (mind.forced_goal.has_value()) {
                hash_scalar(hash, static_cast<std::uint8_t>(*mind.forced_goal));
            }
        }
    }
}

} // namespace aetheria::serialize::detail
