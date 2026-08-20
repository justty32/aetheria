#pragma once

// region_skeleton.h 收斂已量化的穩定 Region 骨架、十階段除錯產物，以及骨架建構／落地入口。

#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/region_civ_stages.h"
#include "core/worldgen/region_climate_stages.h"
#include "core/worldgen/region_config.h"
#include "core/worldgen/region_relief_stages.h"

#include <cstdint>

namespace aetheria::worldgen {

// RegionDefinitionIds 是生成器啟動時一次解析完成的 Ruleset 下標。
// RegionSkeleton 擁有值，populate 只讀取複本。
// 所屬 skeleton 析構後失效；跨 Ruleset 不可沿用。
struct RegionDefinitionIds {
    rules::TerrainId land;
    rules::TerrainId ocean;
    rules::ReliefId plain;
    rules::FeatureId no_feature;
    rules::FeatureId forest;
    rules::FeatureId mine;
    rules::FeatureId oasis;
    rules::FeatureId landmark;
    rules::EdgeId no_edge;
    rules::EdgeId stream;
    rules::EdgeId river;
    rules::EdgeId great_river;
};

// RegionSkeleton 是只含已量化慢變地形的穩定 Region 骨架。
// RegionBuildResult 擁有它，之後可移交世界狀態。
// 所屬擁有者析構或欄位重配後其中參考失效。
struct RegionSkeleton {
    QuantizedElevation elevation;
    ClimateStageOutput climate;
    RiverStageOutput rivers;
    BiomeStageOutput biome;
    FeatureStageOutput features;
    HistoryStageOutput history;
    CityStageOutput cities;
    RoadStageOutput roads;
    RegionDefinitionIds definitions;
};

// RegionBuildResult 同時帶穩定骨架與十階段除錯產物。
// 呼叫端擁有整個回傳值。
// 回傳值析構後所有 stage 與 skeleton 參考失效。
struct RegionBuildResult {
    PlateStageOutput plates;
    HeightStageOutput height;
    ErosionStageOutput erosion;
    ClimateStageOutput climate;
    RiverStageOutput rivers;
    BiomeStageOutput biome;
    FeatureStageOutput features;
    HistoryStageOutput history;
    CityStageOutput cities;
    RoadStageOutput roads;
    RegionSkeleton skeleton;
};

[[nodiscard]] RegionBuildResult build_skeleton(const RegionSlowVariables& slow,
                                               std::uint64_t world_seed,
                                               const rules::Ruleset& ruleset,
                                               const RegionGenerationConfig& config = {});
[[nodiscard]] world::RegionTiles populate(const RegionSkeleton& skeleton,
                                          const RegionFastVariables& fast);

}  // namespace aetheria::worldgen
