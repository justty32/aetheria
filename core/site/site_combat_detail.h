#pragma once

// cohort 戰場分段實作的內部共用介面。

#include "core/site/site_combat.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aetheria::site::combat_detail {

inline constexpr std::int32_t kPermyriad = 10'000;

[[nodiscard]] constexpr std::size_t role_index(CohortRole role) noexcept {
    return static_cast<std::size_t>(role);
}

[[nodiscard]] std::int32_t scaled_nearest(std::int32_t value,
                                          std::int32_t multiplier,
                                          std::int32_t scale) noexcept;
[[nodiscard]] std::vector<SiteCohort> deploy_side(std::int32_t power,
                                                  std::uint8_t side,
                                                  std::uint64_t seed);
[[nodiscard]] std::int32_t desired_range(CohortRole role) noexcept;
[[nodiscard]] std::int32_t move_toward(SiteCohort& cohort,
                                       const SiteCohort& target,
                                       std::uint64_t draw) noexcept;

}  // namespace aetheria::site::combat_detail
