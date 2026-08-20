#pragma once

// gen_stage_ids.h 收斂 Region 生成器內部共用的階段鹽值、階段 id 與世界高度極值。

#include <cstdint>

namespace aetheria::worldgen::detail {

inline constexpr std::uint64_t kRegionSalt = UINT64_C(0xA0761D6478BD642F);
inline constexpr std::uint64_t kPlateStageId = UINT64_C(0x504C415445000001);
inline constexpr std::uint64_t kHeightStageId = UINT64_C(0x4845494748540002);
inline constexpr std::uint64_t kErosionStageId = UINT64_C(0x45524F53494F4E03);
inline constexpr std::uint64_t kClimateStageId = UINT64_C(0x434C494D41544504);
inline constexpr std::uint64_t kRiverStageId = UINT64_C(0x5249564552530005);
inline constexpr std::uint64_t kBiomeStageId = UINT64_C(0x42494F4D45530006);
inline constexpr std::uint64_t kFeatureStageId = UINT64_C(0x4645415455524507);
inline constexpr std::uint64_t kHistoryStageId = UINT64_C(0x484953544F525908);
inline constexpr std::uint64_t kCityStageId = UINT64_C(0x4349544945530009);
inline constexpr std::uint64_t kRoadStageId = UINT64_C(0x524F41445300000A);
inline constexpr double kMinWorldElevation = -4096.0;
inline constexpr double kMaxWorldElevation = 61439.0;

}  // namespace aetheria::worldgen::detail
