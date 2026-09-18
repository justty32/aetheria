#pragma once

// local_path.h：Local 路徑查詢門面；列舉保留原路徑。

#include <cstdint>

namespace aetheria::local {

enum class LocalPathStatus : std::uint8_t {
  Found,
  Coarse,
  NoPath,
  Unknown,
};

enum class LocalPathInteraction : std::uint8_t {
  None,
  OpenDoor,
};

} // namespace aetheria::local

#include "core/local/local_path_types.h"
#include "core/local/local_path_detail.h"
#include "core/local/local_path_impl.h"
