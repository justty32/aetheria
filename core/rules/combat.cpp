// core/rules/combat.cpp：Q28.36 比值 + Q16.16 log2/exp2 的參數化 R^p。

#include "core/rules/combat.h"

#include <array>
#include <bit>
#include <cstdint>
#include <limits>

#include "core/base/check.h"

namespace aetheria::rules {
namespace {

constexpr std::uint64_t kQ32One = std::uint64_t{1} << 32U;
constexpr std::uint64_t kQ32Two = std::uint64_t{2} << 32U;
constexpr std::int64_t kQ16One = std::int64_t{1} << 16U;
constexpr std::int32_t kRatioFractionBits = 36;

[[nodiscard]] std::uint64_t q32_multiply(std::uint64_t left,
                                         std::uint64_t right) noexcept {
    AETH_CHECK(left < kQ32Two);
    AETH_CHECK(right < kQ32Two);
    const auto left_high = left >> 32U;
    const auto right_high = right >> 32U;
    const auto left_low = left & 0xffff'ffffULL;
    const auto right_low = right & 0xffff'ffffULL;
    return (left_high * right_high << 32U) + left_high * right_low +
           right_high * left_low + ((left_low * right_low) >> 32U);
}

[[nodiscard]] std::uint64_t q32_square_root(std::uint64_t value) noexcept {
    AETH_CHECK(value >= kQ32One);
    AETH_CHECK(value <= kQ32Two);
    auto lower = kQ32One;
    auto upper = kQ32Two - 1U;
    while (lower < upper) {
        const auto middle = lower + (upper - lower + 1U) / 2U;
        if (q32_multiply(middle, middle) <= value) {
            lower = middle;
        } else {
            upper = middle - 1U;
        }
    }
    return lower;
}

[[nodiscard]] bool below_binary_limit(std::uint64_t numerator, std::uint64_t denominator,
                                      std::int32_t limit) noexcept {
    if (denominator > (std::numeric_limits<std::uint64_t>::max() >> limit)) {
        return true;
    }
    return numerator <= (denominator << limit);
}

[[nodiscard]] std::uint64_t ratio_fixed(std::int64_t signed_numerator,
                                        std::int64_t signed_denominator,
                                        const CombatRules& rules) noexcept {
    AETH_CHECK(signed_numerator > 0);
    AETH_CHECK(signed_denominator > 0);
    AETH_CHECK(rules.ratio_binary_limit > 0);
    AETH_CHECK(rules.ratio_binary_limit <= 30);
    const auto numerator = static_cast<std::uint64_t>(signed_numerator);
    const auto denominator = static_cast<std::uint64_t>(signed_denominator);
    const bool ratio_within_domain =
        below_binary_limit(numerator, denominator, rules.ratio_binary_limit) &&
        below_binary_limit(denominator, numerator, rules.ratio_binary_limit);
    AETH_CHECK(ratio_within_domain);

    const auto whole = numerator / denominator;
    auto remainder = numerator % denominator;
    std::uint64_t fraction{};
    for (std::int32_t bit = 0; bit < kRatioFractionBits; ++bit) {
        fraction <<= 1U;
        if (remainder >= denominator - remainder) {
            remainder -= denominator - remainder;
            fraction |= 1U;
        } else {
            remainder *= 2U;
        }
    }
    const auto result = (whole << kRatioFractionBits) | fraction;
    AETH_CHECK(result > 0);
    return result;
}

[[nodiscard]] FixedPowerFactor normalize_ratio(std::uint64_t value) noexcept {
    AETH_CHECK(value > 0);
    const auto highest_bit = static_cast<std::int32_t>(std::bit_width(value) - 1U);
    const auto exponent = highest_bit - kRatioFractionBits;
    const auto mantissa_shift = highest_bit - 32;
    const auto mantissa =
        mantissa_shift >= 0 ? value >> mantissa_shift : value << -mantissa_shift;
    AETH_CHECK(mantissa >= kQ32One);
    AETH_CHECK(mantissa < kQ32Two);
    return {mantissa, exponent};
}

[[nodiscard]] std::int64_t log2_q16(std::uint64_t ratio) noexcept {
    auto normalized = normalize_ratio(ratio);
    std::int64_t result = static_cast<std::int64_t>(normalized.binary_exponent) * kQ16One;
    auto value = normalized.mantissa_q32;
    for (std::int32_t bit = 15; bit >= 0; --bit) {
        value = q32_multiply(value, value);
        if (value >= kQ32Two) {
            value >>= 1U;
            result |= std::int64_t{1} << bit;
        }
    }
    return result;
}

[[nodiscard]] std::int64_t divide_nearest(std::int64_t numerator,
                                          std::int32_t denominator) noexcept {
    AETH_CHECK(denominator > 0);
    if (numerator >= 0) {
        return (numerator + denominator / 2) / denominator;
    }
    return -((-numerator + denominator / 2) / denominator);
}

[[nodiscard]] FixedPowerFactor exp2_q16(std::int64_t exponent_q16) noexcept {
    auto integral = exponent_q16 / kQ16One;
    auto fractional = exponent_q16 % kQ16One;
    if (fractional < 0) {
        fractional += kQ16One;
        --integral;
    }

    static const auto roots = [] {
        std::array<std::uint64_t, 16> result{};
        result[0] = q32_square_root(kQ32Two);
        for (std::size_t index = 1; index < result.size(); ++index) {
            result[index] = q32_square_root(result[index - 1]);
        }
        return result;
    }();

    auto mantissa = kQ32One;
    for (std::size_t index = 0; index < roots.size(); ++index) {
        const auto bit = 15U - index;
        if ((static_cast<std::uint64_t>(fractional) & (std::uint64_t{1} << bit)) != 0U) {
            mantissa = q32_multiply(mantissa, roots[index]);
        }
    }
    AETH_CHECK(integral >= std::numeric_limits<std::int32_t>::min());
    AETH_CHECK(integral <= std::numeric_limits<std::int32_t>::max());
    return {mantissa, static_cast<std::int32_t>(integral)};
}

}  // namespace

FixedPowerFactor fixed_ratio_power(std::int64_t numerator, std::int64_t denominator,
                                    CombatExponent exponent,
                                    const CombatRules& rules) noexcept {
    AETH_CHECK(rules.loaded);
    AETH_CHECK(exponent.numerator > 0);
    AETH_CHECK(exponent.denominator > 0);
    AETH_CHECK(static_cast<std::int64_t>(exponent.numerator) <=
               static_cast<std::int64_t>(exponent.denominator) * 4);
    const auto ratio = ratio_fixed(numerator, denominator, rules);
    const auto normalized = normalize_ratio(ratio);
    if (exponent.numerator == exponent.denominator) {
        return normalized;
    }
    if (static_cast<std::int64_t>(exponent.numerator) ==
        static_cast<std::int64_t>(exponent.denominator) * 2) {
        auto mantissa = q32_multiply(normalized.mantissa_q32, normalized.mantissa_q32);
        auto binary_exponent = normalized.binary_exponent * 2;
        if (mantissa >= kQ32Two) {
            mantissa >>= 1U;
            ++binary_exponent;
        }
        return {mantissa, binary_exponent};
    }
    const auto scaled_log = divide_nearest(log2_q16(ratio) * exponent.numerator,
                                           exponent.denominator);
    return exp2_q16(scaled_log);
}

}  // namespace aetheria::rules
