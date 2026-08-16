#pragma once

// gen_hash.h 收斂 Region 生成器內部共用的位元組級雜湊與灰階正規化 helper。

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace aetheria::worldgen::detail {

inline void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
}

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
    const auto bits = static_cast<Unsigned>(value);
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits >> (byte * 8U)));
    }
}

inline void hash_double(std::uint64_t& hash, double value) noexcept {
    hash_scalar(hash, std::bit_cast<std::uint64_t>(value));
}

template <typename Value>
inline void hash_vector(std::uint64_t& hash, const std::vector<Value>& values) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(values.size()));
    for (const auto value : values) {
        hash_scalar(hash, value);
    }
}

inline void hash_double_vector(std::uint64_t& hash, const std::vector<double>& values) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(values.size()));
    for (const auto value : values) {
        hash_double(hash, value);
    }
}

[[nodiscard]] inline std::vector<std::uint8_t>
grayscale_values(const std::vector<double>& values) {
    if (values.empty()) {
        return {};
    }
    const auto [minimum, maximum] = std::minmax_element(values.begin(), values.end());
    const auto span = *maximum - *minimum;
    std::vector<std::uint8_t> pixels;
    pixels.reserve(values.size());
    for (const auto value : values) {
        const auto normalized = span == 0.0 ? 0.0 : (value - *minimum) / span;
        pixels.push_back(static_cast<std::uint8_t>(std::llround(normalized * 255.0)));
    }
    return pixels;
}

}  // namespace aetheria::worldgen::detail
