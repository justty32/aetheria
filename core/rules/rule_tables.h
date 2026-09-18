#pragma once

// 規則表門面；列舉定義保留原路徑以維持原則五的身分。

#include <cstdint>

namespace aetheria::rules {

enum class LocalRoomKind : std::uint8_t {
    Bedroom,
    Kitchen,
    Workshop,
    Shop,
};

// SiteFillZone 目前只列出能由既有快變數驅動的 F1 分區。
enum class SiteFillZone : std::uint8_t {
    Residential,
    Commercial,
};

enum class SiteQuotaDriver : std::uint8_t {
    Population,
    DevelopmentLevel,
};

// UndergroundKind 由 Site 的 structure def 決定 Local 路線 C 種類；None 仍走路線 A。
enum class UndergroundKind : std::uint8_t {
    None,
    Mine,
    Dungeon,
    Ruin,
};

// WorldConnectionType 決定 WorldGraph 通道在 Region 內解析 portal 的規則。
enum class WorldConnectionType : std::uint8_t {
    SeaRoute,
    MountainPass,
    Underground,
    Teleport,
};

}  // namespace aetheria::rules

#include "core/rules/rule_tables_terrain.h"
#include "core/rules/rule_tables_site.h"
#include "core/rules/rule_tables_civilization.h"
