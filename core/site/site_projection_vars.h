#pragma once

// L1→L2 投影的慢／快變數、格網尺寸與持久座標。

#include "core/rules/ruleset.h"
#include "core/site/local_reduction_schema.h"
#include "core/spatial/boundary_profile.h"
#include "core/world/region_tiles.h"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace aetheria::site {

inline constexpr std::uint32_t kSiteWidth = 64;
inline constexpr std::uint32_t kSiteHeight = 64;
inline constexpr std::size_t kSiteTileCount = static_cast<std::size_t>(kSiteWidth) * kSiteHeight;

// SiteSlowVars 是 build_site_skeleton 唯一可讀的 Region tile 狀態。
// 呼叫端擁有值，骨架建構只在呼叫期間借用。
// 型別刻意不含 owner、settlement、site。
struct SiteSlowVars {
    rules::TerrainId base{};
    rules::ReliefId relief{};
    rules::FeatureId feature{};
    std::uint16_t elevation{};
    // 順序固定為 north、east、south、west，與 RegionTiles edge 儲存一致。
    std::array<rules::EdgeId, 4> edges{};

    constexpr bool operator==(const SiteSlowVars&) const noexcept = default;
};

// SiteFastVars 是 populate 專用的 Region tile 狀態。
// 呼叫端擁有值，populate 只在呼叫期間借用。
// 型別刻意不會出現在 build_site_skeleton 簽章。
struct SiteFastVars {
    world::FactionId owner{};
    world::SettlementTier settlement{world::SettlementTier::None};
    world::SiteState site;
    world::PopulationReduction::Value population{};
    world::DevelopmentLevelReduction::Value development_level{};
    world::FoodStockReduction::Value food_stock{};
    world::ProductionStockReduction::Value production_stock{};
    world::DefenseValue defense{};
    world::DamageValue damage{};
    world::OrderReduction::Value order{};

    constexpr bool operator==(const SiteFastVars&) const noexcept = default;
};

// SiteProjectionVars 是從單一 Region tile 一次切出的慢／快變數值。
// 呼叫端擁有值；兩邊後續可分別傳遞，沒有共享可變狀態。
struct SiteProjectionVars {
    SiteSlowVars slow;
    SiteFastVars fast;
};

// SiteXY 是單一 Site 64×64 格網內的持久座標。
// 呼叫端擁有值；值本身永不失效。
struct SiteXY {
    std::uint16_t x{};
    std::uint16_t y{};

    constexpr auto operator<=>(const SiteXY&) const noexcept = default;

    template <typename Archive> void serialize(Archive& archive) { archive(x, y); }
};

}  // namespace aetheria::site
