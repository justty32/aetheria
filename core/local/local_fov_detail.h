#pragma once

// local_fov_detail.h：視野掃描與射線追蹤共用的內部宣告。

#include "core/local/local_fov.h"

namespace aetheria::local::detail::fov {

enum class RayState : std::uint8_t;

// 同一回 FOV 的射線會重複穿越相同有向邊；以相對座標保存查詢結果，
// 避免反覆進入 CrossZoneRuntime，但不把 cache 帶到下一回合。
class EdgeStateCache {
public:
  EdgeStateCache(const runtime::CrossZoneRuntime &runtime,
                 const rules::Ruleset &ruleset,
                 const DoorStateQuery &door_states, std::int32_t radius);

  [[nodiscard]] RayState query(LocalLocation from, std::int32_t relative_x,
                               std::int32_t relative_y,
                               spatial::BoundarySide side);

private:
  const runtime::CrossZoneRuntime &runtime_;
  const rules::Ruleset &ruleset_;
  const DoorStateQuery &door_states_;
  std::int32_t radius_;
  std::size_t extent_;
  std::vector<RayState> states_;
};

void append_visible_ray(FovResult &result, EdgeStateCache &edge_states,
                        LocalLocation origin, std::int32_t dx,
                        std::int32_t dy, LocalLocation target);

} // namespace aetheria::local::detail::fov
