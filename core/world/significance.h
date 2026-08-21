#pragma once

// significance.h 定義實體與事件共用、可依作用範圍比較的唯一重要性等級。

#include <cstdint>

namespace aetheria::world {

enum class Significance : std::uint8_t {
    Ambient,
    Local,
    Site,
    Region,
    World,
};

[[nodiscard]] constexpr bool reaches(Significance value, Significance threshold) noexcept {
    return value >= threshold;
}

static_assert(Significance::Ambient < Significance::Local);
static_assert(Significance::Local < Significance::Site);
static_assert(Significance::Site < Significance::Region);
static_assert(Significance::Region < Significance::World);

}  // namespace aetheria::world
