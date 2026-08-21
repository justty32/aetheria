#pragma once

// site_build_loop_detail.h 是城建命令、經濟結算與流水線之間的內部介面。

#include "core/site/site_build_loop.h"

namespace aetheria::site::build_detail {

[[nodiscard]] const rules::CityBuildingDef& require_definition(
    const rules::Ruleset& ruleset, std::string_view id);
[[nodiscard]] std::uint32_t simulate_hour(CityBuildState& state,
                                          const rules::Ruleset& ruleset,
                                          SiteAdvanceReport& report);

}  // namespace aetheria::site::build_detail
