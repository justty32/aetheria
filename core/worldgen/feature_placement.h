#pragma once

// feature_placement.h：地物生成共用的 FeatureDef.required_terrain 約束。

#include "core/rules/ruleset.h"

namespace aetheria::worldgen::detail {

// 所有目前的程序地物都只會放在陸格；資料若要求水域，生成器不可能兌現。
void validate_land_feature(const rules::Ruleset& ruleset, rules::FeatureId feature);

[[nodiscard]] bool feature_accepts_terrain(const rules::Ruleset& ruleset, rules::FeatureId feature,
                                           rules::TerrainId terrain);

void require_feature_terrain(const rules::Ruleset& ruleset, rules::FeatureId feature,
                             rules::TerrainId terrain);

}  // namespace aetheria::worldgen::detail
