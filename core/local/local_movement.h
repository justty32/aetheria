#pragma once

// local_movement.h：四鄰接探索一步的邊、門鎖與載入狀態判定。

#include "core/local/local_navigation.h"
#include "core/time/tick.h"

#include <cstdint>

namespace aetheria::rules {
class Ruleset;
}

namespace aetheria::runtime {
class CrossZoneRuntime;
}

namespace aetheria::local {

inline constexpr time::Duration kExplorationStride = time::kMinute;

enum class ExplorationStepResult : std::uint8_t {
  Allowed,
  MustOpenDoor,
  BlockedByWall,
  BlockedByLockedDoor,
  Unknown,
};

[[nodiscard]] ExplorationStepResult
assess_exploration_step(const runtime::CrossZoneRuntime &runtime,
                        const rules::Ruleset &ruleset, LocalLocation from,
                        spatial::BoundarySide direction,
                        const DoorStateQuery &door_states = {});

} // namespace aetheria::local
