#pragma once

// local_tiles.h：L3 單一 z 層的 LocalTiles、慢變數骨架輸入與路線 B 輸出。

#include "core/site/site_projection.h"
#include "core/spatial/boundary_profile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace aetheria::local {

inline constexpr std::uint32_t kLocalWidth = 64;
inline constexpr std::uint32_t kLocalHeight = 64;
inline constexpr std::size_t kLocalTileCount =
    static_cast<std::size_t>(kLocalWidth) * kLocalHeight;

// OverlayId 目前只表達路線 B 的程序覆蓋物；0 永遠是空。
enum class OverlayId : std::uint16_t {
    None,
    Road,
    Vegetation,
    Stone,
    ScatteredObject,
};

using EntityId = std::uint64_t;

struct LocalXY {
    std::uint16_t x{};
    std::uint16_t y{};

    constexpr auto operator<=>(const LocalXY&) const noexcept = default;
};

// LocalTiles 是一個 z 層的 64×64 SoA；本里程碑只生成地面層，不建立垂直層容器。
struct LocalTiles {
    std::vector<rules::GroundId> ground;
    std::vector<OverlayId> overlay;
    std::vector<EntityId> occupant;
    std::vector<rules::EdgeId> edges;
    std::vector<std::uint8_t> light;

    [[nodiscard]] bool valid_layout() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    bool operator==(const LocalTiles&) const = default;
};

// LocalSlowVars 是 build_open_local_skeleton 唯一可讀的 Site tile 慢變數。
// 型別刻意不含控制方、損毀、季節、時辰或即時佔用者。
struct LocalSlowVars {
    rules::GroundId ground{};
    OverlayId overlay{OverlayId::None};
    std::array<rules::EdgeId, 4> edges{};
    site::SiteZoning zoning{site::SiteZoning::Open};
    std::optional<rules::BuildingDefId> structure;
    rules::FeatureId feature{};
    std::array<spatial::BoundaryProfile, 4> boundaries;
};

struct LocalFastVars {
    world::FactionId controller{};
    std::uint8_t damage{};
    std::uint8_t season{};
    std::uint8_t hour{};
};

struct OpenLocalSkeleton {
    LocalTiles tiles;
    std::vector<std::uint16_t> elevation;
    std::array<spatial::BoundaryProfile, 4> boundaries;
    std::uint16_t road_path_count{};
    std::uint16_t river_path_count{};
    std::uint16_t scatter_count{};
    std::uint16_t object_count{};

    [[nodiscard]] bool valid_layout() const noexcept;
    bool operator==(const OpenLocalSkeleton&) const = default;
};

[[nodiscard]] std::uint64_t derive_local_seed(std::uint64_t site_seed, std::uint16_t x,
                                              std::uint16_t y) noexcept;

[[nodiscard]] LocalSlowVars project_local_slow_vars(
    const site::SiteProceduralLayer& parent, site::SiteXY coordinate, std::uint64_t site_seed,
    rules::FeatureId feature, const rules::Ruleset& ruleset);

[[nodiscard]] OpenLocalSkeleton build_open_local_skeleton(const LocalSlowVars& slow,
                                                          std::uint64_t local_seed,
                                                          const rules::Ruleset& ruleset);

[[nodiscard]] std::uint64_t hash_open_local_skeleton(
    const OpenLocalSkeleton& skeleton) noexcept;

}  // namespace aetheria::local
