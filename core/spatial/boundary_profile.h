#pragma once

// boundary_profile.h：共用邊界剖面門面；列舉保留原路徑。

#include <cstdint>

namespace aetheria::spatial {

enum class BoundarySide : std::uint8_t {
    North,
    East,
    South,
    West,
};

}  // namespace aetheria::spatial

#include "core/spatial/boundary_profile_types.h"
#include "core/spatial/boundary_profile_detail.h"
#include "core/spatial/boundary_profile_impl.h"
