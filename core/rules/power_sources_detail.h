#pragma once

// 位階來源各職責共用的位階檢查與屬性加總宣告。

#include <cstdint>
#include <string_view>

#include "core/world/significance.h"

namespace aetheria::rules::detail {

[[nodiscard]] std::uint8_t tier_value(world::Significance tier);

[[nodiscard]] std::int32_t checked_add(std::int32_t left, std::int32_t right,
                                       std::string_view field);

}  // namespace aetheria::rules::detail
