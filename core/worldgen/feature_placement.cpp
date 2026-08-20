#include "core/worldgen/feature_placement.h"

#include <stdexcept>
#include <string>

namespace aetheria::worldgen::detail {
namespace {

[[nodiscard]] const rules::FeatureDef& require_feature(const rules::Ruleset& ruleset,
                                                       rules::FeatureId feature) {
    const auto* definition = ruleset.feature(feature);
    if (definition == nullptr) {
        throw std::runtime_error{"地物生成引用不存在的 FeatureDef"};
    }
    return *definition;
}

[[nodiscard]] const rules::TerrainDef& require_terrain(const rules::Ruleset& ruleset,
                                                       rules::TerrainId terrain) {
    const auto* definition = ruleset.terrain(terrain);
    if (definition == nullptr) {
        throw std::runtime_error{"地物生成引用不存在的 TerrainDef"};
    }
    return *definition;
}

}  // namespace

void validate_land_feature(const rules::Ruleset& ruleset, rules::FeatureId feature) {
    const auto& definition = require_feature(ruleset, feature);
    if (!definition.required_terrain.has_value()) {
        return;
    }
    const auto& terrain = require_terrain(ruleset, *definition.required_terrain);
    if ((terrain.flags & rules::kTerrainWaterFlag) != 0) {
        throw std::runtime_error{"FeatureDef " + definition.id + " 的 required_terrain " +
                                 terrain.id + " 無法由陸地地物生成器滿足"};
    }
}

bool feature_accepts_terrain(const rules::Ruleset& ruleset, rules::FeatureId feature,
                             rules::TerrainId terrain) {
    const auto& definition = require_feature(ruleset, feature);
    static_cast<void>(require_terrain(ruleset, terrain));
    return !definition.required_terrain.has_value() || *definition.required_terrain == terrain;
}

void require_feature_terrain(const rules::Ruleset& ruleset, rules::FeatureId feature,
                             rules::TerrainId terrain) {
    if (!feature_accepts_terrain(ruleset, feature, terrain)) {
        const auto& definition = require_feature(ruleset, feature);
        const auto& actual = require_terrain(ruleset, terrain);
        throw std::runtime_error{"FeatureDef " + definition.id + " 的 required_terrain 不接受 " +
                                 actual.id};
    }
}

}  // namespace aetheria::worldgen::detail
