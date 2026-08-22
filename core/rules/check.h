#pragma once

// core/rules/check.h：定義只消耗一次注入 RNG 的 d100 檢定，以及以同一骰
// 餘量查詢的資料驅動效果分段。

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace aetheria::rules {

struct MarginBand {
    std::string id;
    std::int64_t minimum_margin{};
    std::int32_t effect_percent{};
};

struct CheckRules {
    std::vector<MarginBand> margin_bands;
};

struct CheckResult {
    std::uint8_t roll{};
    std::int64_t target{};
    std::int64_t margin{};
    bool success{};
    std::uint16_t band_index{};
    std::int32_t effect_percent{};

    constexpr bool operator==(const CheckResult&) const noexcept = default;
};

[[nodiscard]] CheckResult evaluate_check(std::uint8_t roll, std::int32_t attribute,
                                         std::int32_t modifier, std::int32_t difficulty,
                                         const CheckRules& rules);
[[nodiscard]] CheckResult perform_check(std::mt19937_64& rng, std::int32_t attribute,
                                        std::int32_t modifier, std::int32_t difficulty,
                                        const CheckRules& rules);

}  // namespace aetheria::rules
