#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aetheria::rules {

inline constexpr std::uint32_t kTerrainWaterFlag = UINT32_C(1) << 1U;
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

// BiomeRule 是資料檔中依序第一命中的地形／起伏生成規則。
// Ruleset 擁有所有實例，生成器只借用 const span。
// 所屬 Ruleset 析構後失效；fallback 必須是最後一條。
struct BiomeRule {
    std::int16_t min_temperature_tenths{std::numeric_limits<std::int16_t>::min()};
    std::int16_t max_temperature_tenths{std::numeric_limits<std::int16_t>::max()};
    std::uint16_t min_moisture{};
    std::uint16_t max_moisture{std::numeric_limits<std::uint16_t>::max()};
    std::uint16_t min_elevation{};
    std::uint16_t max_elevation{std::numeric_limits<std::uint16_t>::max()};
    std::uint16_t min_ruggedness{};
    std::uint16_t max_ruggedness{std::numeric_limits<std::uint16_t>::max()};
    TerrainId terrain;
    ReliefId relief;
    bool fallback{};
};

// MovementRules 是資料檔提供的四季整數移動倍率。
// Ruleset 擁有值，移動與尋路系統只讀取複本。
// Ruleset 析構後失效；numerator／denominator 皆以整數計算並向上取整。
struct MovementRules {
    std::array<std::uint16_t, 4> season_numerators{};
    std::uint16_t season_denominator{};
    bool loaded{};
};

// CrossingRule 將河級與道路級資料驅動映射成單一複合 EdgeDef。
// CivilizationRules 擁有所有實例，worldgen 只讀取複本。
// 所屬 Ruleset 析構後失效。
struct CrossingRule {
    EdgeId river;
    EdgeId road;
    EdgeId result;
};

// CivilizationRules 是城市評分、間距、道路工程與渡河查表的資料規則。
// Ruleset 擁有值，worldgen 階段 8～9 只借用 const 參考。
// 所屬 Ruleset 析構後失效；所有分數與成本均為整數。
struct CivilizationRules {
    std::int32_t freshwater_weight{};
    std::int32_t farmland_weight{};
    std::int32_t harbor_weight{};
    std::int32_t defense_weight{};
    std::int32_t resource_weight{};
    std::int32_t bottleneck_weight{};
    std::int32_t extreme_climate_penalty{};
    std::int32_t high_elevation_penalty{};
    std::uint16_t high_elevation_threshold{};
    std::uint16_t target_city_count{};
    std::uint16_t major_city_count{};
    std::uint16_t town_count{};
    std::array<std::uint16_t, 3> minimum_spacing{};
    std::uint8_t bottleneck_radius{};
    std::uint8_t loop_percent{};
    std::uint16_t road_base_cost{};
    std::uint16_t road_terrain_weight{};
    std::uint16_t road_slope_weight{};
    std::uint16_t road_slope_divisor{};
    std::uint16_t road_valley_discount{};
    std::uint16_t road_swamp_penalty{};
    std::uint16_t road_river_crossing_penalty{};
    std::uint16_t road_reuse_numerator{};
    std::uint16_t road_reuse_denominator{};
    std::array<std::uint16_t, 3> road_usage_thresholds{};
    std::array<EdgeId, 3> road_edges{};
    TerrainId swamp_terrain;
    std::vector<CrossingRule> crossings;
    bool loaded{};
};

class RulesetLoader;

// Ruleset 是 TOML 載入後不可變的 def 集合與字串索引。
// 世界狀態擁有它，其餘系統只借用 const Ruleset&。
// 擁有者析構後所有 def 指標、span 與執行期下標失效。
class Ruleset {
    public:
    Ruleset(const Ruleset&) = delete;
    Ruleset& operator=(const Ruleset&) = delete;
    Ruleset(Ruleset&&) noexcept = default;
    Ruleset& operator=(Ruleset&&) noexcept = delete;

    [[nodiscard]] const TerrainDef* terrain(TerrainId id) const noexcept;
    [[nodiscard]] const ReliefDef* relief(ReliefId id) const noexcept;
    [[nodiscard]] const FeatureDef* feature(FeatureId id) const noexcept;
    [[nodiscard]] const EdgeDef* edge(EdgeId id) const noexcept;

    [[nodiscard]] std::optional<TerrainId> find_terrain(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<ReliefId> find_relief(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<FeatureId> find_feature(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<EdgeId> find_edge(std::string_view id) const noexcept;

    [[nodiscard]] std::span<const TerrainDef> terrains() const noexcept { return terrains_; }
    [[nodiscard]] std::span<const ReliefDef> reliefs() const noexcept { return reliefs_; }
    [[nodiscard]] std::span<const FeatureDef> features() const noexcept { return features_; }
    [[nodiscard]] std::span<const EdgeDef> edges() const noexcept { return edges_; }
    [[nodiscard]] std::span<const BiomeRule> biome_rules() const noexcept { return biome_rules_; }
    [[nodiscard]] const MovementRules& movement_rules() const noexcept { return movement_rules_; }
    [[nodiscard]] const CivilizationRules& civilization_rules() const noexcept {
        return civilization_rules_;
    }

    private:
    friend class RulesetLoader;
    Ruleset() = default;

    std::vector<TerrainDef> terrains_;
    std::vector<ReliefDef> reliefs_;
    std::vector<FeatureDef> features_;
    std::vector<EdgeDef> edges_;
    std::vector<BiomeRule> biome_rules_;
    MovementRules movement_rules_;
    CivilizationRules civilization_rules_;
    std::map<std::string, TerrainId, std::less<>> terrain_index_;
    std::map<std::string, ReliefId, std::less<>> relief_index_;
    std::map<std::string, FeatureId, std::less<>> feature_index_;
    std::map<std::string, EdgeId, std::less<>> edge_index_;
};

// RulesetLoader 將一個 data 目錄完整解析成不可變 Ruleset。
// 呼叫端擁有回傳值，loader 不保留狀態。
// load 結束後沒有借用留在 loader 中。
class RulesetLoader {
    public:
    [[nodiscard]] static Ruleset load(const std::filesystem::path& data_directory);
};

}  // namespace aetheria::rules
