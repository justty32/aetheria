#pragma once

#include <aetheria/runtime/cross_zone.h>
#include "core/zone/zone.h"

namespace aetheria::runtime::detail {

[[nodiscard]] constexpr bool valid_tile(local::LocalXY tile) noexcept {
    return tile.x < local::kLocalWidth && tile.y < local::kLocalHeight;
}

[[nodiscard]] const local::LocalTiles* ground_layer(const zone::Zone& value) noexcept;

} // namespace aetheria::runtime::detail
