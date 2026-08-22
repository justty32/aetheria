#include "core/serialize/normalized_state_hash.h"

#include <aetheria/runtime/cross_zone.h>

#include "core/site/site_build_loop.h"
#include "core/site/site_lifecycle.h"
#include "core/world/region_movement.h"
#include "core/world/named_fate.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace aetheria::serialize {
namespace {

inline constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
inline constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= kFnvPrime;
}

template <typename Value, bool = std::is_enum_v<Value>> struct HashBits {
    using Type = Value;
};

template <typename Value> struct HashBits<Value, true> {
    using Type = std::underlying_type_t<Value>;
};

template <typename Value> void hash_scalar(std::uint64_t& hash, Value value) noexcept {
    static_assert(std::is_integral_v<Value> || std::is_enum_v<Value>);
    using Bits = typename HashBits<Value>::Type;
    using Unsigned = std::make_unsigned_t<Bits>;
    auto bits = static_cast<Unsigned>(static_cast<Bits>(value));
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits & UINT8_MAX));
        bits >>= 8U;
    }
}

void hash_string(std::uint64_t& hash, std::string_view value) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(value.size()));
    for (const auto byte : value) {
        hash_byte(hash, static_cast<std::uint8_t>(byte));
    }
}

template <typename Value>
void hash_numeric_vector(std::uint64_t& hash, const std::vector<Value>& values) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(values.size()));
    for (const auto value : values) {
        hash_scalar(hash, value);
    }
}

template <typename Value>
void hash_numeric_span(std::uint64_t& hash, std::span<const Value> values) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(values.size()));
    for (const auto value : values) {
        hash_scalar(hash, value);
    }
}

template <typename Row>
void hash_reduction_row(std::uint64_t& hash, const world::RegionTiles& tiles) noexcept {
    hash_numeric_span(hash, tiles.reduction_values<Row>());
}

template <typename Id, typename Lookup>
void hash_definition_vector(std::uint64_t& hash, const std::vector<Id>& values, Lookup&& lookup,
                            std::string_view kind) {
    hash_scalar(hash, static_cast<std::uint64_t>(values.size()));
    for (const auto value : values) {
        const auto* definition = lookup(value);
        if (definition == nullptr) {
            throw std::runtime_error{"正規化雜湊遇到無效 " + std::string{kind}};
        }
        hash_string(hash, definition->id);
    }
}

template <typename Component> void require_stable_ids(const zone::Zone& zone) {
    for (const auto entity : zone.reg.view<const Component>()) {
        if (!zone.reg.all_of<world::StableId>(entity)) {
            throw std::runtime_error{"權威 unit component 缺少 StableId"};
        }
    }
}

void hash_city_build_state(std::uint64_t& hash, const zone::Zone& zone,
                           const rules::Ruleset& ruleset) {
    const auto states = zone.reg.view<const site::CityBuildState>();
    if (states.size() > 1U) {
        throw std::runtime_error{"正規化雜湊遇到多個 CityBuildState"};
    }
    hash_scalar(hash, static_cast<std::uint64_t>(states.size()));
    if (states.empty()) {
        return;
    }
    const auto& state = states.get<const site::CityBuildState>(*states.begin());
    if (zone::level_of(zone.key) != zone::ZoneLevel::Site ||
        !site::valid_city_build_state(state, ruleset)) {
        throw std::runtime_error{"正規化雜湊遇到無效 CityBuildState"};
    }
    auto buildings = state.buildings;
    std::ranges::sort(buildings, [](const auto& left, const auto& right) {
        return std::tie(left.definition_id, left.origin.y, left.origin.x) <
               std::tie(right.definition_id, right.origin.y, right.origin.x);
    });
    hash_scalar(hash, static_cast<std::uint64_t>(buildings.size()));
    for (const auto& building : buildings) {
        hash_string(hash, building.definition_id);
        hash_scalar(hash, building.origin.x);
        hash_scalar(hash, building.origin.y);
    }
    auto pending = state.pending;
    std::ranges::sort(pending, [](const auto& left, const auto& right) {
        return std::tie(left.definition_id, left.origin.y, left.origin.x,
                        left.remaining_hours) <
               std::tie(right.definition_id, right.origin.y, right.origin.x,
                        right.remaining_hours);
    });
    hash_scalar(hash, static_cast<std::uint64_t>(pending.size()));
    for (const auto& construction : pending) {
        hash_string(hash, construction.definition_id);
        hash_scalar(hash, construction.origin.x);
        hash_scalar(hash, construction.origin.y);
        hash_scalar(hash, construction.remaining_hours);
    }
    hash_scalar(hash, state.economy.population);
    hash_scalar(hash, state.economy.food_stock);
    hash_scalar(hash, state.economy.production_stock);
    hash_scalar(hash, state.economy.population_micro_remainder);
    hash_scalar(hash, state.economy.satisfaction);
    hash_scalar(hash,
                static_cast<std::uint64_t>(state.migration.destroyed_objects.size()));
    for (const auto& object : state.migration.destroyed_objects) {
        hash_scalar(hash, object.kind);
        hash_string(hash, object.definition_id);
        hash_scalar(hash, object.former_coordinate.x);
        hash_scalar(hash, object.former_coordinate.y);
        hash_scalar(hash, object.persistent_type);
        hash_scalar(hash, object.persistent_state);
        hash_scalar(hash, object.aging_seconds);
        hash_scalar(hash, object.remaining_hours);
    }
    hash_scalar(hash, static_cast<std::uint64_t>(state.migration.events.size()));
    for (const auto& event : state.migration.events) {
        hash_scalar(hash, event.old_skeleton_hash);
        hash_scalar(hash, event.new_skeleton_hash);
        hash_scalar(hash, event.retained);
        hash_scalar(hash, event.relocated);
        hash_scalar(hash, event.destroyed);
        hash_string(hash, event.narrative);
    }
}

void hash_fate_stage_one(std::uint64_t& hash, const world::FateStageOne& stage) noexcept {
    hash_scalar(hash, stage.crisis.event_id);
    hash_scalar(hash, stage.crisis.cohort_id);
    hash_scalar(hash, stage.crisis.site_key);
    hash_scalar(hash, stage.crisis.base_loss_basis_points);
    hash_scalar(hash, stage.crisis.relief_basis_points);
    hash_scalar(hash, static_cast<std::int64_t>(stage.crisis.occurred_at));
    hash_string(hash, stage.crisis.place_key);
    hash_scalar(hash, stage.population_before);
    hash_scalar(hash, stage.total_loss);
    hash_scalar(hash, stage.population_after);
}

void hash_named_fate_ledger(std::uint64_t& hash, const zone::Zone& zone) {
    const auto ledgers = zone.reg.view<const world::NamedFateLedger>();
    if (ledgers.size() > 1U) {
        throw std::runtime_error{"正規化雜湊遇到多個 NamedFateLedger"};
    }
    hash_scalar(hash, static_cast<std::uint64_t>(ledgers.size()));
    if (ledgers.empty()) {
        return;
    }
    const auto& ledger = ledgers.get<const world::NamedFateLedger>(*ledgers.begin());
    const auto level = zone::level_of(zone.key);
    if ((level != zone::ZoneLevel::Region && level != zone::ZoneLevel::Site) ||
        !world::valid_named_fate_ledger(ledger)) {
        throw std::runtime_error{"正規化雜湊遇到無效 NamedFateLedger"};
    }
    auto members = ledger.members;
    std::ranges::sort(members, {}, &world::NamedFateMember::entity_uid);
    hash_scalar(hash, static_cast<std::uint64_t>(members.size()));
    for (const auto& member : members) {
        hash_scalar(hash, member.entity_uid);
        hash_scalar(hash, member.cohort_id);
        hash_string(hash, member.name_key);
        hash_scalar(hash, member.significance);
        hash_string(hash, member.significance_reason);
        hash_scalar(hash, member.modifiers.status_basis_points);
        hash_scalar(hash, member.modifiers.wealth_basis_points);
        hash_scalar(hash, member.modifiers.relationship_basis_points);
        hash_scalar(hash, member.modifiers.occupation_basis_points);
        hash_scalar(hash, member.modifiers.vulnerability_basis_points);
        hash_scalar(hash, member.modifiers.district_basis_points);
        hash_scalar(hash, static_cast<std::uint8_t>(member.marked));
        hash_scalar(hash, static_cast<std::uint8_t>(member.rescued));
        hash_scalar(hash, static_cast<std::uint8_t>(member.has_outcome));
        hash_scalar(hash, member.outcome);
    }
    auto pending = ledger.pending;
    std::ranges::sort(pending, [](const auto& left, const auto& right) {
        return std::tie(left.crisis.site_key, left.crisis.event_id, left.crisis.cohort_id) <
               std::tie(right.crisis.site_key, right.crisis.event_id, right.crisis.cohort_id);
    });
    hash_scalar(hash, static_cast<std::uint64_t>(pending.size()));
    for (const auto& stage : pending) {
        hash_fate_stage_one(hash, stage);
    }
    hash_scalar(hash, static_cast<std::uint64_t>(ledger.events.size()));
    for (const auto& event : ledger.events) {
        hash_scalar(hash, event.kind);
        hash_scalar(hash, event.event_id);
        hash_scalar(hash, event.cohort_id);
        hash_scalar(hash, event.entity_uid);
        hash_string(hash, event.place_key);
        hash_string(hash, event.person_key);
        hash_string(hash, event.template_key);
        hash_scalar(hash, event.outcome);
    }
}

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

}  // namespace

std::uint64_t normalized_state_hash(const zone::Zone& zone, const rules::Ruleset& ruleset) {
    auto hash = kFnvOffset;
    hash_scalar(hash, zone::value_of(zone.key));
    hash_diplomacy_state(hash, zone, ruleset);

    const auto clocks = zone.reg.view<const world::TurnClock>();
    if (clocks.size() > 1U) {
        throw std::runtime_error{"正規化雜湊至多允許一個 TurnClock"};
    }
    hash_scalar(hash, static_cast<std::uint64_t>(clocks.size()));
    if (!clocks.empty()) {
        hash_scalar(
            hash,
            static_cast<std::int64_t>(clocks.get<const world::TurnClock>(*clocks.begin()).now));
    }

    if (const auto* region = std::get_if<zone::RegionPayload>(&zone.payload)) {
        hash_scalar(hash, static_cast<std::uint64_t>(region->layers.size()));
        for (const auto& [z, tiles] : region->layers) {
            if (!tiles.valid_layout()) {
                throw std::runtime_error{"正規化雜湊遇到無效 RegionTiles layout"};
            }
            hash_scalar(hash, z);
            hash_scalar(hash, tiles.width);
            hash_scalar(hash, tiles.height);
            hash_definition_vector(
                hash, tiles.base, [&](rules::TerrainId id) { return ruleset.terrain(id); },
                "TerrainId");
            hash_definition_vector(
                hash, tiles.relief, [&](rules::ReliefId id) { return ruleset.relief(id); },
                "ReliefId");
            hash_definition_vector(
                hash, tiles.feature, [&](rules::FeatureId id) { return ruleset.feature(id); },
                "FeatureId");
            hash_numeric_vector(hash, tiles.temperature);
            hash_numeric_vector(hash, tiles.moisture);
            hash_numeric_vector(hash, tiles.elevation);
            hash_definition_vector(
                hash, tiles.edges, [&](rules::EdgeId id) { return ruleset.edge(id); }, "EdgeId");
            hash_scalar(hash, static_cast<std::uint64_t>(tiles.owner.size()));
            for (const auto owner : tiles.owner) {
                hash_scalar(hash, owner);
            }
            hash_numeric_vector(hash, tiles.settlement);
            hash_numeric_vector(hash, tiles.defense);
            hash_numeric_vector(hash, tiles.damage);
            std::apply(
                [&](auto... row) {
                    (hash_reduction_row<std::remove_cvref_t<decltype(row)>>(hash, tiles), ...);
                },
                world::RegionReductionRows{});
            hash_scalar(hash, static_cast<std::uint64_t>(tiles.portals.size()));
            for (const auto& portal : tiles.portals) {
                hash_scalar(hash, portal.tile.x);
                hash_scalar(hash, portal.tile.y);
                hash_scalar(hash, rules::value_of(portal.channel));
            }
            hash_scalar(hash, static_cast<std::uint64_t>(tiles.site.size()));
            for (const auto& site : tiles.site) {
                hash_scalar(hash, static_cast<std::uint8_t>(site.ever_realized));
            }
        }
    } else if (const auto* site_payload = std::get_if<zone::SitePayload>(&zone.payload)) {
        hash_scalar(hash, static_cast<std::uint64_t>(zone.payload.index()));
        if (!site::valid_persistent_layer(site_payload->layers.persistent)) {
            throw std::runtime_error{"正規化雜湊遇到無效 SitePersistentLayer"};
        }
        std::vector<site::PersistentBuilding> buildings = site_payload->layers.persistent.buildings;
        std::ranges::sort(buildings, [](const auto& left, const auto& right) {
            return std::tie(left.tile.y, left.tile.x, left.type, left.state) <
                   std::tie(right.tile.y, right.tile.x, right.type, right.state);
        });
        hash_scalar(hash, static_cast<std::uint64_t>(buildings.size()));
        for (const auto& building : buildings) {
            hash_scalar(hash, building.tile.x);
            hash_scalar(hash, building.tile.y);
            hash_scalar(hash, building.type);
            hash_scalar(hash, building.state);
            hash_scalar(hash, building.aging_seconds);
        }
    } else {
        hash_scalar(hash, static_cast<std::uint64_t>(zone.payload.index()));
    }

    hash_city_build_state(hash, zone, ruleset);
    hash_named_fate_ledger(hash, zone);
    require_stable_ids<world::RegionPosition>(zone);
    require_stable_ids<world::MovementPoints>(zone);
    require_stable_ids<world::RegionMoveCommand>(zone);
    require_stable_ids<runtime::LocalPosition>(zone);
    std::vector<std::pair<std::uint64_t, entt::entity>> entities;
    for (const auto entity : zone.reg.view<const world::StableId>()) {
        entities.emplace_back(zone.reg.get<const world::StableId>(entity).uid, entity);
    }
    std::ranges::sort(entities);
    hash_scalar(hash, static_cast<std::uint64_t>(entities.size()));
    std::uint64_t previous{};
    bool has_previous{};
    for (const auto [stable_id, entity] : entities) {
        if (has_previous && stable_id == previous) {
            throw std::runtime_error{"正規化雜湊遇到重複 StableId：" + std::to_string(stable_id)};
        }
        previous = stable_id;
        has_previous = true;
        hash_scalar(hash, stable_id);

        const auto* position = zone.reg.try_get<const world::RegionPosition>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(position != nullptr));
        if (position != nullptr) {
            hash_scalar(hash, position->z);
            hash_scalar(hash, position->tile.x);
            hash_scalar(hash, position->tile.y);
        }
        const auto* local_position = zone.reg.try_get<const runtime::LocalPosition>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(local_position != nullptr));
        if (local_position != nullptr) {
            hash_scalar(hash, local_position->tile.x);
            hash_scalar(hash, local_position->tile.y);
        }
        const auto* points = zone.reg.try_get<const world::MovementPoints>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(points != nullptr));
        if (points != nullptr) {
            hash_scalar(hash, points->current);
            hash_scalar(hash, points->per_xun);
        }
        const auto* command = zone.reg.try_get<const world::RegionMoveCommand>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(command != nullptr));
        if (command != nullptr) {
            hash_scalar(hash, command->target.x);
            hash_scalar(hash, command->target.y);
            hash_scalar(hash, static_cast<std::uint8_t>(command->collected));
        }
    }
    return hash;
}

}  // namespace aetheria::serialize
