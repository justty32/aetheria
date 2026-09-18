#pragma once

// 三層資料型別與投影入口；列舉定義保留在原門面。
#include "core/site/site_projection.h"

namespace aetheria::site {

using BuildingType = rules::PersistentBuildingType;

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

// SiteOrderState 保存歸約公式的四個真實因子；order 本身只存在 Region。
struct SiteOrderState {
    std::uint16_t garrison_coverage{};
    std::uint16_t patrol_coverage{};
    std::uint16_t bandit_pressure{};
    std::uint16_t refugee_pressure{};

    template <typename Archive> void serialize(Archive& archive) {
        archive(garrison_coverage, patrol_coverage, bandit_pressure, refugee_pressure);
    }
    constexpr bool operator==(const SiteOrderState&) const noexcept = default;
};

struct PersistentNamedNpc {
    std::uint64_t uid{};
    std::string person_name_key;
    std::string last_seen_place_name_key;
    bool known_to_player{};
    bool missing{};

    template <typename Archive> void serialize(Archive& archive) {
        archive(uid, person_name_key, last_seen_place_name_key, known_to_player, missing);
    }
    bool operator==(const PersistentNamedNpc&) const = default;
};

struct PersistentDungeon {
    std::uint64_t uid{};
    std::string place_name_key;
    bool cleared{};
    std::uint16_t depth{};

    template <typename Archive> void serialize(Archive& archive) {
        archive(uid, place_name_key, cleared, depth);
    }
    bool operator==(const PersistentDungeon&) const = default;
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
    std::optional<SiteOrderState> order;
    std::string place_name_key;
    std::vector<PersistentNamedNpc> named_npcs;
    std::vector<PersistentDungeon> dungeons;

    bool operator==(const SitePersistentLayer&) const = default;

    template <typename Archive> void serialize(Archive& archive) {
        archive(buildings);
        bool has_order = order.has_value();
        archive(has_order);
        if constexpr (Archive::is_loading::value) {
            if (has_order) {
                order.emplace();
            } else {
                order.reset();
            }
        }
        if (has_order) {
            archive(*order);
        }
        archive(place_name_key, named_npcs, dungeons);
    }
};

// SiteVolatileLayer 是由持久層與規則重建、永不存檔的資料層；M2.1 刻意為空。
struct SiteVolatileLayer {};

// SiteLayers 以三個具名型別固定資料所有權邊界，而不是靠欄位註解分類。
struct SiteLayers {
    SiteProceduralLayer procedural;
    SitePersistentLayer persistent;
    SiteVolatileLayer volatile_state;
    // Local 歸約是 Site 權威快變數；空 delta 不會覆寫這些值。
    LocalReductionLayer local_reductions{kSiteTileCount};

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
