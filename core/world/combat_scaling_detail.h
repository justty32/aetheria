#pragma once

// 戰鬥抽樣與升層配置共用的側別 seed salt。

#include <cstdint>

namespace aetheria::world::detail {

inline constexpr std::uint64_t kSideASalt = UINT64_C(0x4f2c6b997c31e9d5);
inline constexpr std::uint64_t kSideBSalt = UINT64_C(0xa7d13e4825bc906f);

}  // namespace aetheria::world::detail
