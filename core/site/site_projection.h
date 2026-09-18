#pragma once

// site_projection.h 定義 L1→L2
// 投影的慢／快變數界面、城區程序骨架與三層資料型別。

#include <cstdint>

namespace aetheria::site {

enum class SiteZoning : std::uint8_t {
    Open,
    Residential,
    Commercial,
};

enum class BuildingState : std::uint8_t {
    Active,
    Idle,
    Derelict,
    Ruined,
};

enum class ProceduralBuildingDamage : std::uint8_t {
    Intact,
    Rubble,
    Burned,
};

}  // namespace aetheria::site

#include "core/site/site_projection_vars.h"
#include "core/site/site_projection_layers.h"
