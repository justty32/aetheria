// 外交持久狀態的 v15 位元流：關係、條約、理由與戰爭事件。

#include "core/serialize/zone_diplomacy_codec.h"

#include <cereal/types/string.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace aetheria::serialize::detail {
namespace {

constexpr std::uint64_t kMaximumDiplomacyRecords = UINT64_C(1'000'000);

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

[[nodiscard]] std::uint64_t load_count(cereal::PortableBinaryInputArchive& archive,
                                       const char* kind) {
    std::uint64_t count{};
    archive(count);
    if (count > kMaximumDiplomacyRecords) {
        throw std::runtime_error{std::string{"外交存檔的"} + kind + "筆數過大"};
    }
    return count;
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
}

std::optional<world::WorldDiplomacyState>
load_diplomacy(cereal::PortableBinaryInputArchive& archive, const rules::Ruleset& ruleset) {
    bool present{};
    archive(present);
    if (!present) {
        return std::nullopt;
    }
    world::DiplomacyPersistentState state;
    archive(state.faction_count, state.world_seed);
    if (state.faction_count != ruleset.civilization_rules().factions.faction_count) {
        throw std::runtime_error{"外交存檔的勢力數與目前 Ruleset 不符"};
    }
    const auto extent = static_cast<std::uint64_t>(state.faction_count) + 1U;
    const auto relation_count = load_count(archive, "關係");
    if (relation_count != extent * extent) {
        throw std::runtime_error{"外交存檔的關係矩陣尺寸不符"};
    }
    state.relations.resize(static_cast<std::size_t>(relation_count));
    for (auto& relation : state.relations) {
        archive(relation.favor, relation.trust, relation.fear, relation.grievance);
    }

    const auto treaty_count = load_count(archive, "條約");
    state.treaties.reserve(static_cast<std::size_t>(treaty_count));
    for (std::uint64_t index = 0; index < treaty_count; ++index) {
        std::string id;
        std::uint16_t first{};
        std::uint16_t second{};
        std::int64_t started{};
        bool has_expiry{};
        std::int64_t expiry{};
        archive(id, first, second, started, has_expiry, expiry);
        const auto def = ruleset.find_treaty(id);
        if (!def.has_value()) {
            throw std::runtime_error{"外交存檔含目前 Ruleset 不存在的條約 id：" + id};
        }
        state.treaties.push_back({
            .def = *def,
            .parties = {world::FactionId{first}, world::FactionId{second}},
            .started = time::Tick{started},
            .expires_at = has_expiry ? std::optional<time::Tick>{time::Tick{expiry}} : std::nullopt,
        });
    }

    const auto claim_count = load_count(archive, "宣戰理由");
    state.casus_belli.reserve(static_cast<std::size_t>(claim_count));
    for (std::uint64_t index = 0; index < claim_count; ++index) {
        std::uint16_t owner{};
        std::uint16_t target{};
        std::string id;
        std::int64_t granted{};
        std::int64_t expires{};
        archive(owner, target, id, granted, expires);
        const auto def = ruleset.find_casus_belli(id);
        if (!def.has_value()) {
            throw std::runtime_error{"外交存檔含目前 Ruleset 不存在的宣戰理由 id：" + id};
        }
        state.casus_belli.push_back({
            .owner = world::FactionId{owner},
            .target = world::FactionId{target},
            .def = *def,
            .granted_at = time::Tick{granted},
            .expires_at = time::Tick{expires},
        });
    }

    const auto war_count = load_count(archive, "戰爭");
    state.wars.reserve(static_cast<std::size_t>(war_count));
    for (std::uint64_t index = 0; index < war_count; ++index) {
        std::uint16_t first{};
        std::uint16_t second{};
        bool has_cause{};
        std::string cause_id;
        std::int64_t started{};
        world::WarEvent war;
        archive(first, second, has_cause, cause_id, started, war.war_score, war.weariness[0],
                war.weariness[1], war.active);
        war.participants = {world::FactionId{first}, world::FactionId{second}};
        war.started = time::Tick{started};
        if (has_cause) {
            const auto cause = ruleset.find_casus_belli(cause_id);
            if (!cause.has_value()) {
                throw std::runtime_error{"外交存檔含目前 Ruleset 不存在的戰爭理由 id：" + cause_id};
            }
            war.cause = *cause;
        } else if (!cause_id.empty()) {
            throw std::runtime_error{"外交存檔的無理由戰爭帶有理由 id"};
        }
        state.wars.push_back(war);
    }
    return world::WorldDiplomacyState::restore(std::move(state), ruleset);
}

} // namespace aetheria::serialize::detail
