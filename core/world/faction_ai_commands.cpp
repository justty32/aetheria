// 玩家與勢力 AI 共用的命令執行及防禦同盟回應。

#include "core/world/faction_ai.h"

#include <algorithm>
#include <stdexcept>

namespace aetheria::world {
namespace {

[[nodiscard]] bool same_active_war(const WarEvent& war, FactionId first,
                                   FactionId second) noexcept {
    return war.active &&
           ((war.participants[0] == first && war.participants[1] == second) ||
            (war.participants[0] == second && war.participants[1] == first));
}

} // namespace

void execute_faction_command(WorldDiplomacyState& world,
                             const ai::FactionCommand& command,
                             ai::PowerChange power_change, time::Tick now,
                             const rules::Ruleset& ruleset) {
    const auto issuer = FactionId{command.issuer};
    world.adjust_faction_truth(issuer, power_change.military, power_change.economic);
    if (command.target == 0) {
        return;
    }
    const auto target = FactionId{command.target};
    switch (command.kind) {
    case ai::FactionActionKind::DeclareWar:
        if (!std::ranges::any_of(world.wars(), [&](const auto& war) {
                return same_active_war(war, issuer, target);
            })) {
            static_cast<void>(world.declare_war(issuer, target, std::nullopt, now));
        }
        break;
    case ai::FactionActionKind::OfferAlliance: {
        const auto alliance = ruleset.find_treaty("treaty.defensive_alliance");
        if (!alliance.has_value()) {
            throw std::logic_error{"Ruleset 缺少防禦同盟"};
        }
        const auto already_allied = std::ranges::any_of(world.treaties(), [&](const auto& treaty) {
            return treaty.def == *alliance &&
                   ((treaty.parties[0] == issuer && treaty.parties[1] == target) ||
                    (treaty.parties[0] == target && treaty.parties[1] == issuer)) &&
                   (!treaty.expires_at.has_value() || now < *treaty.expires_at);
        });
        if (!already_allied) {
            static_cast<void>(world.start_treaty(*alliance, issuer, target, now));
        }
        break;
    }
    case ai::FactionActionKind::PayTribute:
        world.adjust_relation(target, issuer, {.favor = 200, .trust = 100});
        break;
    case ai::FactionActionKind::Develop:
    case ai::FactionActionKind::Prepare:
    case ai::FactionActionKind::Expand:
    case ai::FactionActionKind::StatisticalProgress:
        break;
    }
}

void answer_defensive_alliance_call(WorldDiplomacyState& world, FactionId ally,
                                    FactionId defended, FactionId attacker,
                                    bool joins, time::Tick now,
                                    const rules::Ruleset& ruleset) {
    const auto alliance = ruleset.find_treaty("treaty.defensive_alliance");
    if (!alliance.has_value()) {
        throw std::logic_error{"Ruleset 缺少防禦同盟"};
    }
    const auto obligated = std::ranges::any_of(world.treaties(), [&](const auto& treaty) {
        return treaty.def == *alliance &&
               ((treaty.parties[0] == ally && treaty.parties[1] == defended) ||
                (treaty.parties[0] == defended && treaty.parties[1] == ally)) &&
               (!treaty.expires_at.has_value() || now < *treaty.expires_at);
    });
    if (!obligated) {
        throw std::invalid_argument{"勢力沒有有效防禦同盟義務"};
    }
    if (joins) {
        if (!std::ranges::any_of(world.wars(), [&](const auto& war) {
                return same_active_war(war, ally, attacker);
            })) {
            static_cast<void>(world.declare_war(ally, attacker, std::nullopt, now, true));
        }
    } else {
        world.adjust_relation(defended, ally, {.trust = -5000});
    }
}

} // namespace aetheria::world
