#include "core/worldgen/field_redistribution.h"

#include <utility>

namespace aetheria::worldgen {

ErosionStageOutput redistribute(ErosionStageOutput field,
                                const ElevationRedistributionParams &params) {
  return redistribute(std::move(field), params,
                      [](auto &, const auto &) noexcept {});
}

RiverStageOutput redistribute(RiverStageOutput field,
                              const MoistureRedistributionParams &params) {
  return redistribute(std::move(field), params,
                      [](auto &, const auto &) noexcept {});
}

} // namespace aetheria::worldgen
