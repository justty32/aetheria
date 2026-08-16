#pragma once

// region_seed.h 收斂 Region 生成器的種子推導函式宣告。

#include <cstdint>

namespace aetheria::worldgen {

[[nodiscard]] std::uint64_t splitmix64(std::uint64_t value) noexcept;
[[nodiscard]] std::uint64_t derive_stage_seed(std::uint64_t world_seed,
                                              std::uint64_t stage_id) noexcept;
[[nodiscard]] std::uint64_t derive_region_seed(std::uint64_t world_seed,
                                               std::uint32_t region_id) noexcept;
[[nodiscard]] std::uint64_t derive_region_stage_seed(std::uint64_t world_seed,
                                                     std::uint32_t region_id,
                                                     std::uint64_t stage_id) noexcept;

}  // namespace aetheria::worldgen
