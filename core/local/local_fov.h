#pragma once

// local_fov.h：以邊遮蔽、可跨已載入 Local zone 的逐格視野查詢。

#include "core/local/local_navigation.h"

#include <cstdint>
#include <vector>

namespace aetheria::rules {
class Ruleset;
}

namespace aetheria::runtime {
class CrossZoneRuntime;
}

namespace aetheria::local {

struct FovParameters {
  std::uint16_t dark_radius{2};
  std::uint16_t bright_radius{12};

  constexpr bool operator==(const FovParameters &) const noexcept = default;
};

struct FovResult {
  std::vector<LocalLocation> visible;
  bool degraded{};

  bool operator==(const FovResult &) const = default;
};

// visible 依 dy、dx 掃描順序固定；未知 tile／edge 不可見並把 degraded 設為
// true。
[[nodiscard]] FovResult calculate_fov(const runtime::CrossZoneRuntime &runtime,
                                      const rules::Ruleset &ruleset,
                                      LocalLocation origin,
                                      FovParameters parameters = {},
                                      const DoorStateQuery &door_states = {});

[[nodiscard]] bool is_visible(const FovResult &fov,
                              LocalLocation location) noexcept;

} // namespace aetheria::local
