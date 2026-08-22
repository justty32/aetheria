#pragma once

// field_redistribution.h 是高度與濕度進入量化／分類前的分布重整接縫。

#include "core/worldgen/region_climate_stages.h"
#include "core/worldgen/region_relief_stages.h"

#include <utility>

namespace aetheria::worldgen {

// 測試可傳入區域性的非恆等 transform 驗證接縫；正式管線只呼叫下方的預設
// overload。
template <typename Stage, typename Params, typename Transform>
[[nodiscard]] Stage redistribute(Stage field, const Params &params,
                                 Transform &&transform) {
  std::forward<Transform>(transform)(field, params);
  return field;
}

// 高度：熱力侵蝕完成後、uint16 公尺量化前。
// 濕度：河流回灌完成後、biome 分類與 uint8 tile 量化前。
// 兩個預設 overload 本輪皆為逐位不變的 identity。
[[nodiscard]] ErosionStageOutput
redistribute(ErosionStageOutput field,
             const ElevationRedistributionParams &params);
[[nodiscard]] RiverStageOutput
redistribute(RiverStageOutput field,
             const MoistureRedistributionParams &params);

} // namespace aetheria::worldgen
