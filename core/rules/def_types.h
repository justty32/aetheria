#pragma once

// core/rules/def_types.h：Ruleset 的基礎 id 型別、flag 常數與地形／地物／邊 def。

#include <cstdint>
#include <optional>
#include <string>

namespace aetheria::rules {

inline constexpr std::uint32_t kTerrainWaterFlag = UINT32_C(1) << 1U;
inline constexpr std::uint32_t kGroundWaterFlag = UINT32_C(1);
inline constexpr std::uint32_t kFeatureRuinFlag = UINT32_C(1) << 4U;
inline constexpr std::uint32_t kEdgeRoadFlag = UINT32_C(1);
inline constexpr std::uint32_t kEdgeRiverFlag = UINT32_C(1) << 1U;
inline constexpr std::uint32_t kEdgeBridgeFlag = UINT32_C(1) << 2U;

// TerrainId 是 Ruleset 中 TerrainDef 的強型別下標。
// Ruleset 配發其值，tile 只保存值的複本。
// Ruleset 不變時值持續有效；跨存檔須先由字串 id 重映射。
enum class TerrainId : std::uint16_t {};

// ReliefId 是 Ruleset 中 ReliefDef 的強型別下標。
// Ruleset 配發其值，tile 只保存值的複本。
// Ruleset 不變時值持續有效；跨存檔須先由字串 id 重映射。
enum class ReliefId : std::uint16_t {};

// FeatureId 是 Ruleset 中 FeatureDef 的強型別下標。
// Ruleset 配發其值，tile 只保存值的複本。
// Ruleset 不變時值持續有效；跨存檔須先由字串 id 重映射。
enum class FeatureId : std::uint16_t {};

// EdgeId 是 Ruleset 中 EdgeDef 的強型別下標。
// Ruleset 配發其值，tile 只保存值的複本。
// Ruleset 不變時值持續有效；跨存檔須先由字串 id 重映射。
enum class EdgeId : std::uint16_t {};

// GroundId 是 Ruleset 中 Site GroundDef 的強型別下標。
// Ruleset 配發其值，程序生成的 Site ground 只保存值的複本。
// Ruleset 不變時值持續有效；程序層不進存檔。
enum class GroundId : std::uint16_t {};

// BuildingDefId 是 Ruleset 中程序建築 def 的強型別下標。
// 程序層只保存值的複本且不進存檔。
enum class BuildingDefId : std::uint16_t {};

// WorldConnectionId 是 world_graph.toml 中手工配發的穩定通道識別。
// WorldGraphConnection 與 RegionPortal 保存其值的複本。
// 同一份世界圖中不得重複，存檔跨版本時不做下標重映射。
enum class WorldConnectionId : std::uint32_t {};

[[nodiscard]] constexpr std::uint16_t value_of(TerrainId id) noexcept {
    return static_cast<std::uint16_t>(id);
}
[[nodiscard]] constexpr std::uint16_t value_of(ReliefId id) noexcept {
    return static_cast<std::uint16_t>(id);
}
[[nodiscard]] constexpr std::uint16_t value_of(FeatureId id) noexcept {
    return static_cast<std::uint16_t>(id);
}
[[nodiscard]] constexpr std::uint16_t value_of(EdgeId id) noexcept {
    return static_cast<std::uint16_t>(id);
}
[[nodiscard]] constexpr std::uint16_t value_of(GroundId id) noexcept {
    return static_cast<std::uint16_t>(id);
}
[[nodiscard]] constexpr std::uint16_t value_of(BuildingDefId id) noexcept {
    return static_cast<std::uint16_t>(id);
}
[[nodiscard]] constexpr std::uint32_t value_of(WorldConnectionId id) noexcept {
    return static_cast<std::uint32_t>(id);
}

// Yield 是 terrain 提供的四種整數產出。
// TerrainDef 擁有它。
// 所屬 Ruleset 析構後失效。
struct Yield {
    std::int32_t food{};
    std::int32_t production{};
    std::int32_t wealth{};
    std::int32_t mana{};

    constexpr bool operator==(const Yield&) const noexcept = default;
};

// VisualRef 是 core 不解讀、原樣交給顯示層的資源鍵。
// 所屬 def 擁有字串。
// 所屬 Ruleset 析構後失效。
struct VisualRef {
    std::string key;

    bool operator==(const VisualRef&) const noexcept = default;
};

// TerrainDef 描述一種基底地形。
// Ruleset 擁有所有實例。
// 所屬 Ruleset 析構後失效。
struct TerrainDef {
    std::string id;
    std::string name_key;
    std::int32_t move_cost{};
    Yield yield;
    std::uint32_t flags{};
    VisualRef visual;
};

// ReliefDef 描述一種地形起伏。
// Ruleset 擁有所有實例。
// 所屬 Ruleset 析構後失效。
struct ReliefDef {
    std::string id;
    std::string name_key;
    std::int32_t move_cost{};
    std::uint32_t flags{};
    VisualRef visual;
};

// FeatureDef 描述一種地物及其可選基底地形引用。
// Ruleset 擁有所有實例。
// 所屬 Ruleset 析構後失效。
struct FeatureDef {
    std::string id;
    std::string name_key;
    std::int32_t move_cost{};
    std::uint32_t flags{};
    VisualRef visual;
    std::optional<TerrainId> required_terrain;
};

// EdgeDef 描述一種位於相鄰 tile 之間的河流或道路。
// Ruleset 擁有所有實例。
// 所屬 Ruleset 析構後失效。
struct EdgeDef {
    std::string id;
    std::string name_key;
    std::int32_t move_cost{};
    std::uint32_t flags{};
    VisualRef visual;
};

// GroundDef 描述 Site 格的程序地面種類。
// Ruleset 擁有所有實例。
// 所屬 Ruleset 析構後失效。
struct GroundDef {
    std::string id;
    std::string name_key;
    std::int32_t move_cost{};
    std::uint32_t flags{};
    VisualRef visual;
};

// TerrainGroundMapping 是 Region TerrainDef 到 Site GroundDef 的資料驅動投影規則。
// Ruleset 依 TerrainId 順序擁有所有項目；rough_ground 只提供最小地表變化。
// 所屬 Ruleset 析構後失效。
struct TerrainGroundMapping {
    TerrainId terrain;
    GroundId ground;
    GroundId rough_ground;
};

}  // namespace aetheria::rules
