#pragma once

// 城市與上古選址共用的貪婪分級／間距選擇 helper；只在 core/worldgen 內使用。

#include "core/worldgen/region_civ_stages.h"

#include <array>
#include <cstdint>
#include <vector>

namespace aetheria::worldgen::detail {

struct SettlementSelectionParameters {
    std::uint16_t target_count{};
    std::uint16_t city_count{};
    std::uint16_t town_count{};
    std::array<std::uint16_t, 3> minimum_spacing{};
    std::int32_t minimum_score{};
};

[[nodiscard]] std::vector<CitySite>
select_city_sites(const QuantizedElevation& elevation, const CityStageOutput& scored,
                  std::uint64_t stage_seed, const SettlementSelectionParameters& parameters);

}  // namespace aetheria::worldgen::detail
