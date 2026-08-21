#pragma once

// site_projection.h 定義 L1→L2 投影的慢／快變數界面、城區程序骨架與三層資料型別。

#include "core/rules/ruleset.h"
#include "core/spatial/boundary_profile.h"
#include "core/world/region_tiles.h"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
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

enum class SiteZoning : std::uint8_t {
    Open,
    Residential,
    Commercial,
};

enum class BuildingType : std::uint8_t {
    SettlementHall,
};

enum class BuildingState : std::uint8_t {
    Active,
    Idle,
    Derelict,
    Ruined,
};

// PersistentBuilding 是 M2.2 的最小真實持久物件，不構成建築系統。
struct PersistentBuilding {
    SiteXY tile;
    BuildingType type{BuildingType::SettlementHall};
    BuildingState state{BuildingState::Active};
    std::uint32_t aging_seconds{};

    constexpr bool operator==(const PersistentBuilding&) const noexcept = default;

    template <typename Archive> void serialize(Archive& archive) {
        archive(tile, type, state, aging_seconds);
    }
};

using SiteBoundarySide = spatial::BoundarySide;

// SiteGate 是 Region 道路 crossing 在 Site 邊界上的落點。
struct SiteGate {
    SiteBoundarySide side{SiteBoundarySide::North};
    SiteXY tile;
    rules::EdgeId kind{};

    constexpr bool operator==(const SiteGate&) const noexcept = default;
};

// SiteEdgeRef 指向一條 Site 格邊；F3/F5 metadata 只保存 canonical 的單側參考。
struct SiteEdgeRef {
    SiteXY tile;
    SiteBoundarySide side{SiteBoundarySide::North};

    constexpr bool operator==(const SiteEdgeRef&) const noexcept = default;
};

enum class ProceduralBuildingDamage : std::uint8_t {
    Intact,
    Rubble,
    Burned,
};

// ProceduralBuilding 是 F2 底稿中的矩形建築，不進存檔；持久層疊加時優先於它。
struct ProceduralBuilding {
    rules::BuildingDefId def{};
    SiteXY origin;
    std::uint8_t width{};
    std::uint8_t height{};
    SiteBoundarySide frontage{SiteBoundarySide::North};
    ProceduralBuildingDamage damage{ProceduralBuildingDamage::Intact};

    constexpr bool operator==(const ProceduralBuilding&) const noexcept = default;
};

// SiteBlock 是次級街道遞迴二分後的矩形街廓，不包含切分街道本身。
struct SiteBlock {
    SiteXY origin;
    std::uint16_t width{};
    std::uint16_t height{};

    [[nodiscard]] constexpr std::uint32_t area() const noexcept {
        return static_cast<std::uint32_t>(width) * height;
    }
    constexpr bool operator==(const SiteBlock&) const noexcept = default;
};

// SiteSkeleton 是 64×64 的 S1～S4 程序層：地形、道路、街廓與可建地。
// SiteProceduralLayer 擁有它；不進存檔。
// vector 重配或擁有者析構後其中參考失效。
struct SiteSkeleton {
    std::vector<rules::GroundId> ground;
    std::vector<rules::EdgeId> edges;
    std::vector<std::uint16_t> elevation;
    std::vector<std::uint8_t> water;
    std::vector<std::uint8_t> roads;
    std::vector<std::uint8_t> buildable;
    SiteXY city_center;
    std::vector<SiteGate> gates;
    std::vector<SiteBlock> blocks;

    [[nodiscard]] bool valid_layout() const noexcept;
    [[nodiscard]] bool is_water(SiteXY tile) const noexcept;
    [[nodiscard]] bool is_road(SiteXY tile) const noexcept;
    [[nodiscard]] bool is_buildable(SiteXY tile) const noexcept;
    bool operator==(const SiteSkeleton&) const = default;
};

// SiteProceduralLayer 是可由 site_seed + 慢變數重算、永不存檔的資料。
struct SiteProceduralLayer {
    SiteSkeleton skeleton;
    // F3 的最終邊層；初值是 skeleton.edges，填充只改這份，不回寫骨架。
    std::vector<rules::EdgeId> edges;
    std::vector<SiteZoning> zoning;
    std::vector<SiteZoning> block_zoning;
    std::vector<ProceduralBuilding> buildings;
    std::vector<SiteEdgeRef> wall_edges;
    std::vector<SiteEdgeRef> wall_gates;
    std::vector<SiteEdgeRef> wall_breaches;
    std::uint8_t wall_ring_count{};

    [[nodiscard]] bool valid_layout() const noexcept;
};

// SitePersistentLayer 是唯一允許進 Site 存檔的資料層。
struct SitePersistentLayer {
    std::vector<PersistentBuilding> buildings;

    bool operator==(const SitePersistentLayer&) const = default;

    template <typename Archive> void serialize(Archive& archive) { archive(buildings); }
};

// SiteVolatileLayer 是由持久層與規則重建、永不存檔的資料層；M2.1 刻意為空。
struct SiteVolatileLayer {};

// SiteLayers 以三個具名型別固定資料所有權邊界，而不是靠欄位註解分類。
struct SiteLayers {
    SiteProceduralLayer procedural;
    SitePersistentLayer persistent;
    SiteVolatileLayer volatile_state;

    template <typename Layer> [[nodiscard]] Layer& get() noexcept {
        if constexpr (std::is_same_v<Layer, SiteProceduralLayer>) {
            return procedural;
        } else if constexpr (std::is_same_v<Layer, SitePersistentLayer>) {
            return persistent;
        } else {
            static_assert(std::is_same_v<Layer, SiteVolatileLayer>);
            return volatile_state;
        }
    }

    template <typename Layer> [[nodiscard]] const Layer& get() const noexcept {
        if constexpr (std::is_same_v<Layer, SiteProceduralLayer>) {
            return procedural;
        } else if constexpr (std::is_same_v<Layer, SitePersistentLayer>) {
            return persistent;
        } else {
            static_assert(std::is_same_v<Layer, SiteVolatileLayer>);
            return volatile_state;
        }
    }
};

[[nodiscard]] SiteProjectionVars split_site_vars(const world::RegionTiles& tiles,
                                                 world::RegionXY coordinate);

// 與 interface-world-mid.md 的公式一致：
// splitmix64(world_seed ^ region_id ^ (y << 16 | x))。
[[nodiscard]] std::uint64_t derive_site_seed(std::uint64_t world_seed, std::uint32_t region_id,
                                             std::uint16_t x, std::uint16_t y) noexcept;

[[nodiscard]] SiteSkeleton build_site_skeleton(const SiteSlowVars& slow, std::uint64_t site_seed,
                                               const rules::Ruleset& ruleset);
[[nodiscard]] SiteProceduralLayer populate(SiteSkeleton skeleton, const SiteFastVars& fast,
                                           const rules::Ruleset& ruleset);
[[nodiscard]] bool valid_persistent_layer(const SitePersistentLayer& layer) noexcept;
[[nodiscard]] std::uint64_t hash_site_skeleton(const SiteSkeleton& skeleton) noexcept;
[[nodiscard]] std::uint64_t hash_site_fill(const SiteProceduralLayer& procedural) noexcept;

}  // namespace aetheria::site
