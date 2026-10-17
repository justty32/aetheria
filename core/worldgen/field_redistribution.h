#pragma once

// field_redistribution.h 是高度與濕度進入量化／分類前的分布重整接縫。

#include "core/worldgen/region_climate_stages.h"
#include "core/worldgen/region_relief_stages.h"

#include <concepts>
#include <utility>

namespace aetheria::worldgen {

// 測試可傳入區域性的非恆等 transform 驗證接縫；正式管線只呼叫下方的預設
// overload。
template <typename Stage, typename Params, typename Transform>
  requires std::invocable<Transform, Stage &, const Params &>
[[nodiscard]] Stage redistribute(Stage field, const Params &params,
                                 Transform &&transform) {
  std::forward<Transform>(transform)(field, params);
  return field;
}

// 高度：熱力侵蝕完成後、uint16 公尺量化前；目前仍是逐位不變的 identity。
[[nodiscard]] ErosionStageOutput
redistribute(ErosionStageOutput field,
             const ElevationRedistributionParams &params);

// 濕度：河流回灌完成後、biome 分類與 uint8 tile 量化前。
// 只對陸地格做整數 rank remap（Freeciv adjust_int_map 式的 CDF 等化）：依
// (moisture, lake, flow, 較低的 filled_elevation) 排序，把陸地濕度攤平到原本的
// [陸地最小, 陸地最大] 區間。削頂在 uint16 天花板的同值群由後三個鍵拆開——
// 單純的值 LUT 拆不開同值群；四鍵全等的格取同一值，結果與格子編號無關。
// 海洋格原樣保留。
[[nodiscard]] RiverStageOutput
redistribute(RiverStageOutput field, const QuantizedElevation &elevation,
             const MoistureRedistributionParams &params);

} // namespace aetheria::worldgen
