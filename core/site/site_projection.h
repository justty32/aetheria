#pragma once

// site_projection.h 定義 L1→L2 投影的慢／快變數界面、最小程序骨架與三層資料型別。

#include "core/rules/ruleset.h"
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
    Settlement,
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

    constexpr bool operator==(const PersistentBuilding&) const noexcept = default;

    template <typename Archive> void serialize(Archive& archive) { archive(tile, type, state); }
};

// SiteSkeleton 是 64×64 的最小程序層，只含 ground 與每格四向 edges。
// SiteProceduralLayer 擁有它；不進存檔。
// vector 重配或擁有者析構後其中參考失效。
struct SiteSkeleton {
    std::vector<rules::GroundId> ground;
    std::vector<rules::EdgeId> edges;
    std::vector<std::uint8_t> buildable;

    [[nodiscard]] bool valid_layout() const noexcept;
    [[nodiscard]] bool is_buildable(SiteXY tile) const noexcept;
    bool operator==(const SiteSkeleton&) const = default;
};

// SiteProceduralLayer 是可由 site_seed + 慢變數重算、永不存檔的資料。
struct SiteProceduralLayer {
    SiteSkeleton skeleton;
    std::vector<SiteZoning> zoning;

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
[[nodiscard]] SiteProceduralLayer populate(SiteSkeleton skeleton, const SiteFastVars& fast);
[[nodiscard]] bool valid_persistent_layer(const SitePersistentLayer& layer) noexcept;
[[nodiscard]] std::uint64_t hash_site_skeleton(const SiteSkeleton& skeleton) noexcept;

}  // namespace aetheria::site
