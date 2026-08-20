#pragma once

// 歷史層的 MST 古道鋪設 helper；河邊跳過並保留截斷遮罩，供驗收排除渡河 step。

#include "core/worldgen/region_civ_stages.h"

#include <cstdint>
#include <vector>

namespace aetheria::worldgen::detail {

struct AncientRoadOutput {
    std::vector<rules::EdgeId> edges;
    std::vector<RoadConnection> connections;
    std::vector<std::uint8_t> skipped_river_edges;
};

[[nodiscard]] AncientRoadOutput
build_ancient_roads(const QuantizedElevation& elevation, const ClimateStageOutput& climate,
                    const RiverStageOutput& rivers, const BiomeStageOutput& biome,
                    const FeatureStageOutput& features, const CityStageOutput& ancient_sites,
                    const RegionDefinitionIds& definitions, const rules::Ruleset& ruleset,
                    bool canonicalize_city_order);

}  // namespace aetheria::worldgen::detail
