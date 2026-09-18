#pragma once

#include "core/serialize/normalized_state_hash.h"

#include <type_traits>
#include <string_view>

namespace aetheria::serialize::detail {

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept;
void hash_string(std::uint64_t& hash, std::string_view value) noexcept;

void hash_city_build_state(std::uint64_t& hash, const zone::Zone& zone,
                           const rules::Ruleset& ruleset);

void hash_named_fate_ledger(std::uint64_t& hash, const zone::Zone& zone);

void hash_diplomacy_state(std::uint64_t& hash, const zone::Zone& zone,
                          const rules::Ruleset& ruleset);

void hash_spatial_payload(std::uint64_t& hash, const zone::Zone& zone,
                          const rules::Ruleset& ruleset);
void hash_units(std::uint64_t& hash, const zone::Zone& zone);

template <typename Value, bool = std::is_enum_v<Value>> struct HashBits {
    using Type = Value;
};

template <typename Value> struct HashBits<Value, true> {
    using Type = std::underlying_type_t<Value>;
};

template <typename Value> inline void hash_scalar(std::uint64_t& hash, Value value) noexcept {
    static_assert(std::is_integral_v<Value> || std::is_enum_v<Value>);
    using Bits = typename HashBits<Value>::Type;
    using Unsigned = std::make_unsigned_t<Bits>;
    auto bits = static_cast<Unsigned>(static_cast<Bits>(value));
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits & UINT8_MAX));
        bits >>= 8U;
    }
}

} // namespace aetheria::serialize::detail
