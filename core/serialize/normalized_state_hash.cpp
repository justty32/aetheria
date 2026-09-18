#include "core/serialize/normalized_state_hash.h"

#include <aetheria/runtime/cross_zone.h>

#include "core/site/site_build_loop.h"
#include "core/site/site_lifecycle.h"
#include "core/local/local_navigation.h"
#include "core/world/army_state.h"
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

#include "core/serialize/normalized_state_hash_detail.h"

namespace aetheria::serialize {
namespace {

using namespace detail;

inline constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
inline constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

void hash_local_doors(std::uint64_t& hash, const zone::Zone& zone) {
    const auto states = zone.reg.view<const local::LocalDoorState>();
    if (states.size() > 1U ||
        (!states.empty() && zone::level_of(zone.key) != zone::ZoneLevel::Local)) {
        throw std::runtime_error{"正規化雜湊遇到無效 LocalDoorState"};
    }
    hash_scalar(hash, static_cast<std::uint64_t>(states.size()));
    if (states.empty()) {
        return;
    }
    const auto& opened =
        states.get<const local::LocalDoorState>(*states.begin()).opened;
    hash_scalar(hash, static_cast<std::uint64_t>(opened.size()));
    for (const auto& edge : opened) {
        hash_scalar(hash, zone::value_of(edge.first.zone));
        hash_scalar(hash, edge.first.tile.x);
        hash_scalar(hash, edge.first.tile.y);
        hash_scalar(hash, zone::value_of(edge.second.zone));
        hash_scalar(hash, edge.second.tile.x);
        hash_scalar(hash, edge.second.tile.y);
    }
}

} // namespace

namespace detail {

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= kFnvPrime;
}

void hash_string(std::uint64_t& hash, std::string_view value) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(value.size()));
    for (const auto byte : value) {
        hash_byte(hash, static_cast<std::uint8_t>(byte));
    }
}

} // namespace detail

std::uint64_t normalized_state_hash(const zone::Zone& zone, const rules::Ruleset& ruleset) {
    using namespace detail;
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

    hash_spatial_payload(hash, zone, ruleset);

    hash_city_build_state(hash, zone, ruleset);
    hash_named_fate_ledger(hash, zone);
    hash_local_doors(hash, zone);
    hash_units(hash, zone);
    return hash;
}

} // namespace aetheria::serialize
