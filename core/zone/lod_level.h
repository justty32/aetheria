#pragma once

#include <cstdint>

namespace aetheria::zone {

// LodLevel 是 zone 或下層 Site 當前模擬解析度。
// 所屬世界狀態擁有其值。
// 值本身永不失效，Absent 代表未載入。
enum class LodLevel : std::uint8_t {
    Full,
    Coarse,
    Frozen,
    Absent,
};

}  // namespace aetheria::zone
