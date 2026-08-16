#include "core/rules/ruleset.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace aetheria::rules {
namespace {

[[nodiscard]] const toml::array& read_array(const std::filesystem::path& path,
                                            std::string_view section) {
    static thread_local toml::table document;
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error{"Ruleset 檔案不存在：" + path.string()};
    }
    try {
        document = toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() + "：" +
                                 std::string{error.description()}};
    }
    const auto* entries = document[section].as_array();
    if (entries == nullptr) {
        throw std::runtime_error{"Ruleset 缺少 [[" + std::string{section} + "]] 區段：" +
                                 path.string()};
    }
    return *entries;
}

[[nodiscard]] const toml::array& read_defs(const std::filesystem::path& path) {
    return read_array(path, "defs");
}

[[nodiscard]] const toml::table& require_table(const toml::node& node,
                                               const std::filesystem::path& path) {
    const auto* table = node.as_table();
    if (table == nullptr) {
        throw std::runtime_error{"Ruleset defs 項目不是 table：" + path.string()};
    }
    return *table;
}

[[nodiscard]] std::string require_string(const toml::table& table, std::string_view field,
                                         const std::filesystem::path& path) {
    const auto value = table[field].value<std::string>();
    if (!value.has_value() || value->empty()) {
        throw std::runtime_error{"Ruleset 缺少非空字串欄位 " + std::string{field} + "：" +
                                 path.string()};
    }
    return *value;
}

[[nodiscard]] std::int64_t require_integer(const toml::table& table, std::string_view field,
                                           const std::filesystem::path& path) {
    const auto value = table[field].value<std::int64_t>();
    if (!value.has_value()) {
        throw std::runtime_error{"Ruleset 缺少整數欄位 " + std::string{field} + "：" +
                                 path.string()};
    }
    return *value;
}

[[nodiscard]] std::int32_t require_int32(const toml::table& table, std::string_view field,
                                         const std::filesystem::path& path) {
    const auto value = require_integer(table, field, path);
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error{"Ruleset 整數欄位超出 int32 " + std::string{field} + "：" +
                                 path.string()};
    }
    return static_cast<std::int32_t>(value);
}

template <typename Value>
[[nodiscard]] Value optional_bounded_integer(const toml::table& table, std::string_view field,
                                             Value fallback, const std::filesystem::path& path) {
    const auto value = table[field].value<std::int64_t>();
    if (!value.has_value()) {
        return fallback;
    }
    if (*value < static_cast<std::int64_t>(std::numeric_limits<Value>::min()) ||
        *value > static_cast<std::int64_t>(std::numeric_limits<Value>::max())) {
        throw std::runtime_error{"Ruleset 整數欄位超出範圍 " + std::string{field} + "：" +
                                 path.string()};
    }
    return static_cast<Value>(*value);
}

template <typename Def>
void read_common(const toml::table& table, const std::filesystem::path& path, Def& def,
                 bool allow_zero_move_cost = false) {
    def.id = require_string(table, "id", path);
    def.name_key = require_string(table, "name_key", path);
    const auto move_cost = require_integer(table, "move_cost", path);
    if (move_cost < (allow_zero_move_cost ? 0 : 1) ||
        move_cost > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error{"Ruleset move_cost 超出允許範圍：" + def.id};
    }
    def.move_cost = static_cast<std::int32_t>(move_cost);
    const auto flags = require_integer(table, "flags", path);
    if (flags < 0 ||
        static_cast<std::uint64_t>(flags) > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error{"Ruleset flags 超出 uint32：" + def.id};
    }
    def.flags = static_cast<std::uint32_t>(flags);
    def.visual.key = require_string(table, "visual", path);
}

void register_global_id(std::set<std::string, std::less<>>& ids, const std::string& id,
                        std::string_view required_prefix) {
    if (!id.starts_with(required_prefix)) {
        throw std::runtime_error{"Ruleset id 缺少類型前綴 " + std::string{required_prefix} + "：" +
                                 id};
    }
    if (!ids.insert(id).second) {
        throw std::runtime_error{"Ruleset 全域 id 重複：" + id};
    }
}

template <typename Id, typename Def> [[nodiscard]] Id append_def(std::vector<Def>& defs, Def def) {
    if (defs.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error{"Ruleset 單一 def 類型超過 uint16 容量"};
    }
    const auto id = static_cast<Id>(defs.size());
    defs.push_back(std::move(def));
    return id;
}

template <typename Def, typename Id>
[[nodiscard]] const Def* lookup(std::span<const Def> defs, Id id) noexcept {
    const auto index = static_cast<std::size_t>(value_of(id));
    return index < defs.size() ? &defs[index] : nullptr;
}

template <typename Id>
[[nodiscard]] std::optional<Id> find_id(const std::map<std::string, Id, std::less<>>& index,
                                        std::string_view id) noexcept {
    const auto found = index.find(id);
    return found == index.end() ? std::nullopt : std::optional<Id>{found->second};
}

}  // namespace

const TerrainDef* Ruleset::terrain(TerrainId id) const noexcept { return lookup(terrains(), id); }
const ReliefDef* Ruleset::relief(ReliefId id) const noexcept { return lookup(reliefs(), id); }
const FeatureDef* Ruleset::feature(FeatureId id) const noexcept { return lookup(features(), id); }
const EdgeDef* Ruleset::edge(EdgeId id) const noexcept { return lookup(edges(), id); }

std::optional<TerrainId> Ruleset::find_terrain(std::string_view id) const noexcept {
    return find_id(terrain_index_, id);
}
std::optional<ReliefId> Ruleset::find_relief(std::string_view id) const noexcept {
    return find_id(relief_index_, id);
}
std::optional<FeatureId> Ruleset::find_feature(std::string_view id) const noexcept {
    return find_id(feature_index_, id);
}
std::optional<EdgeId> Ruleset::find_edge(std::string_view id) const noexcept {
    return find_id(edge_index_, id);
}

Ruleset RulesetLoader::load(const std::filesystem::path& data_directory) {
    Ruleset result;
    std::set<std::string, std::less<>> global_ids;
    std::vector<std::pair<std::size_t, std::string>> feature_terrain_references;

    const auto terrain_path = data_directory / "terrain.toml";
    for (const auto& node : read_defs(terrain_path)) {
        const auto& table = require_table(node, terrain_path);
        TerrainDef def;
        read_common(table, terrain_path, def);
        const auto* yield = table["yield"].as_table();
        if (yield == nullptr) {
            throw std::runtime_error{"TerrainDef 缺少 yield 區段：" + def.id};
        }
        def.yield.food = require_int32(*yield, "food", terrain_path);
        def.yield.production = require_int32(*yield, "production", terrain_path);
        def.yield.wealth = require_int32(*yield, "wealth", terrain_path);
        def.yield.mana = require_int32(*yield, "mana", terrain_path);
        register_global_id(global_ids, def.id, "terrain.");
        const auto id = append_def<TerrainId>(result.terrains_, std::move(def));
        result.terrain_index_.emplace(result.terrains_.back().id, id);
    }

    const auto relief_path = data_directory / "relief.toml";
    for (const auto& node : read_defs(relief_path)) {
        const auto& table = require_table(node, relief_path);
        ReliefDef def;
        read_common(table, relief_path, def);
        register_global_id(global_ids, def.id, "relief.");
        const auto id = append_def<ReliefId>(result.reliefs_, std::move(def));
        result.relief_index_.emplace(result.reliefs_.back().id, id);
    }

    const auto feature_path = data_directory / "feature.toml";
    for (const auto& node : read_defs(feature_path)) {
        const auto& table = require_table(node, feature_path);
        FeatureDef def;
        read_common(table, feature_path, def, true);
        const auto reference = table["required_terrain"].value<std::string>();
        register_global_id(global_ids, def.id, "feature.");
        const auto id = append_def<FeatureId>(result.features_, std::move(def));
        result.feature_index_.emplace(result.features_.back().id, id);
        if (reference.has_value()) {
            feature_terrain_references.emplace_back(value_of(id), *reference);
        }
    }

    const auto edge_path = data_directory / "edges.toml";
    for (const auto& node : read_defs(edge_path)) {
        const auto& table = require_table(node, edge_path);
        EdgeDef def;
        read_common(table, edge_path, def, true);
        register_global_id(global_ids, def.id, "edge.");
        const auto id = append_def<EdgeId>(result.edges_, std::move(def));
        result.edge_index_.emplace(result.edges_.back().id, id);
    }

    for (const auto& [feature_index, terrain_string_id] : feature_terrain_references) {
        const auto terrain = result.find_terrain(terrain_string_id);
        if (!terrain.has_value()) {
            throw std::runtime_error{"FeatureDef 引用不存在的 terrain id：" + terrain_string_id};
        }
        result.features_.at(feature_index).required_terrain = *terrain;
    }

    const auto biome_path = data_directory / "biomes.toml";
    if (std::filesystem::is_regular_file(biome_path)) {
        bool saw_fallback{};
        for (const auto& node : read_array(biome_path, "rules")) {
            const auto& table = require_table(node, biome_path);
            if (saw_fallback) {
                throw std::runtime_error{"Biome fallback 後不得再有規則：" + biome_path.string()};
            }
            BiomeRule rule;
            rule.fallback = table["fallback"].value_or(false);
            rule.min_temperature_tenths = optional_bounded_integer<std::int16_t>(
                table, "min_temperature_tenths", rule.min_temperature_tenths, biome_path);
            rule.max_temperature_tenths = optional_bounded_integer<std::int16_t>(
                table, "max_temperature_tenths", rule.max_temperature_tenths, biome_path);
            rule.min_moisture = optional_bounded_integer<std::uint16_t>(
                table, "min_moisture", rule.min_moisture, biome_path);
            rule.max_moisture = optional_bounded_integer<std::uint16_t>(
                table, "max_moisture", rule.max_moisture, biome_path);
            rule.min_elevation = optional_bounded_integer<std::uint16_t>(
                table, "min_elevation", rule.min_elevation, biome_path);
            rule.max_elevation = optional_bounded_integer<std::uint16_t>(
                table, "max_elevation", rule.max_elevation, biome_path);
            rule.min_ruggedness = optional_bounded_integer<std::uint16_t>(
                table, "min_ruggedness", rule.min_ruggedness, biome_path);
            rule.max_ruggedness = optional_bounded_integer<std::uint16_t>(
                table, "max_ruggedness", rule.max_ruggedness, biome_path);
            if (rule.min_temperature_tenths > rule.max_temperature_tenths ||
                rule.min_moisture > rule.max_moisture || rule.min_elevation > rule.max_elevation ||
                rule.min_ruggedness > rule.max_ruggedness) {
                throw std::runtime_error{"BiomeRule 範圍上下界顛倒：" + biome_path.string()};
            }
            const auto terrain_string = require_string(table, "terrain", biome_path);
            const auto relief_string = require_string(table, "relief", biome_path);
            const auto terrain = result.find_terrain(terrain_string);
            const auto relief = result.find_relief(relief_string);
            if (!terrain.has_value() || !relief.has_value()) {
                throw std::runtime_error{"BiomeRule 引用不存在的 def：terrain=" + terrain_string +
                                         " relief=" + relief_string};
            }
            rule.terrain = *terrain;
            rule.relief = *relief;
            saw_fallback = rule.fallback;
            result.biome_rules_.push_back(rule);
        }
        if (result.biome_rules_.empty() || !saw_fallback) {
            throw std::runtime_error{"BiomeRule 最後一條必須是 fallback：" + biome_path.string()};
        }
    }

    const auto movement_path = data_directory / "movement.toml";
    if (std::filesystem::is_regular_file(movement_path)) {
        toml::table movement;
        try {
            movement = toml::parse_file(movement_path.string());
        } catch (const toml::parse_error& error) {
            throw std::runtime_error{"Ruleset TOML 格式錯誤：" + movement_path.string() + "：" +
                                     std::string{error.description()}};
        }
        const auto* numerators = movement["season_numerators"].as_array();
        const auto denominator = movement["season_denominator"].value<std::int64_t>();
        if (numerators == nullptr || numerators->size() != 4 || !denominator.has_value() ||
            *denominator <= 0 || *denominator > UINT16_MAX) {
            throw std::runtime_error{"movement.toml 的四季倍率格式無效"};
        }
        for (std::size_t index = 0; index < numerators->size(); ++index) {
            const auto numerator = (*numerators)[index].value<std::int64_t>();
            if (!numerator.has_value() || *numerator <= 0 || *numerator > UINT16_MAX) {
                throw std::runtime_error{"movement.toml 的四季倍率必須是正整數"};
            }
            result.movement_rules_.season_numerators[index] =
                static_cast<std::uint16_t>(*numerator);
        }
        result.movement_rules_.season_denominator = static_cast<std::uint16_t>(*denominator);
        result.movement_rules_.loaded = true;
    }

    const auto civilization_path = data_directory / "civilization.toml";
    if (std::filesystem::is_regular_file(civilization_path)) {
        toml::table civilization;
        try {
            civilization = toml::parse_file(civilization_path.string());
        } catch (const toml::parse_error& error) {
            throw std::runtime_error{"Ruleset TOML 格式錯誤：" + civilization_path.string() +
                                     "：" + std::string{error.description()}};
        }
        auto& rules = result.civilization_rules_;
        rules.freshwater_weight = require_int32(civilization, "freshwater_weight", civilization_path);
        rules.farmland_weight = require_int32(civilization, "farmland_weight", civilization_path);
        rules.harbor_weight = require_int32(civilization, "harbor_weight", civilization_path);
        rules.defense_weight = require_int32(civilization, "defense_weight", civilization_path);
        rules.resource_weight = require_int32(civilization, "resource_weight", civilization_path);
        rules.bottleneck_weight = require_int32(civilization, "bottleneck_weight", civilization_path);
        rules.extreme_climate_penalty =
            require_int32(civilization, "extreme_climate_penalty", civilization_path);
        rules.high_elevation_penalty =
            require_int32(civilization, "high_elevation_penalty", civilization_path);
        auto positive_u16 = [&](std::string_view field) {
            const auto value = require_integer(civilization, field, civilization_path);
            if (value <= 0 || value > UINT16_MAX) {
                throw std::runtime_error{"civilization.toml 欄位必須是正 uint16：" +
                                         std::string{field}};
            }
            return static_cast<std::uint16_t>(value);
        };
        rules.high_elevation_threshold = positive_u16("high_elevation_threshold");
        rules.target_city_count = positive_u16("target_city_count");
        rules.major_city_count = positive_u16("major_city_count");
        rules.town_count = positive_u16("town_count");
        const auto bottleneck_radius = positive_u16("bottleneck_radius");
        const auto loop_percent = positive_u16("loop_percent");
        if (bottleneck_radius > 8 || loop_percent > UINT8_MAX) {
            throw std::runtime_error{"civilization.toml 半徑或環路比例超出 uint8"};
        }
        rules.bottleneck_radius = static_cast<std::uint8_t>(bottleneck_radius);
        rules.loop_percent = static_cast<std::uint8_t>(loop_percent);
        rules.road_base_cost = positive_u16("road_base_cost");
        rules.road_terrain_weight = positive_u16("road_terrain_weight");
        rules.road_slope_weight = positive_u16("road_slope_weight");
        rules.road_slope_divisor = positive_u16("road_slope_divisor");
        rules.road_valley_discount = positive_u16("road_valley_discount");
        rules.road_swamp_penalty = positive_u16("road_swamp_penalty");
        rules.road_river_crossing_penalty = positive_u16("road_river_crossing_penalty");
        rules.road_reuse_numerator = positive_u16("road_reuse_numerator");
        rules.road_reuse_denominator = positive_u16("road_reuse_denominator");
        if (rules.major_city_count + rules.town_count > rules.target_city_count ||
            rules.loop_percent < 10 || rules.loop_percent > 20 ||
            rules.road_reuse_numerator >= rules.road_reuse_denominator) {
            throw std::runtime_error{"civilization.toml 的數量、環路或道路折扣無效"};
        }
        auto read_u16_array = [&](std::string_view field, auto& target) {
            const auto* values = civilization[field].as_array();
            if (values == nullptr || values->size() != target.size()) {
                throw std::runtime_error{"civilization.toml 陣列尺寸無效：" +
                                         std::string{field}};
            }
            for (std::size_t index = 0; index < target.size(); ++index) {
                const auto value = (*values)[index].value<std::int64_t>();
                if (!value.has_value() || *value <= 0 || *value > UINT16_MAX) {
                    throw std::runtime_error{"civilization.toml 陣列值無效：" +
                                             std::string{field}};
                }
                target[index] = static_cast<std::uint16_t>(*value);
            }
        };
        read_u16_array("minimum_spacing", rules.minimum_spacing);
        read_u16_array("road_usage_thresholds", rules.road_usage_thresholds);
        if (!std::is_sorted(rules.minimum_spacing.begin(), rules.minimum_spacing.end()) ||
            !std::is_sorted(rules.road_usage_thresholds.begin(),
                            rules.road_usage_thresholds.end())) {
            throw std::runtime_error{"civilization.toml 間距與道路門檻必須遞增"};
        }
        const auto swamp_id = require_string(civilization, "swamp_terrain", civilization_path);
        const auto swamp = result.find_terrain(swamp_id);
        if (!swamp.has_value()) {
            throw std::runtime_error{"civilization.toml 引用不存在的 swamp terrain：" + swamp_id};
        }
        rules.swamp_terrain = *swamp;
        const auto* road_edges = civilization["road_edges"].as_array();
        if (road_edges == nullptr || road_edges->size() != rules.road_edges.size()) {
            throw std::runtime_error{"civilization.toml road_edges 必須有三級"};
        }
        for (std::size_t index = 0; index < rules.road_edges.size(); ++index) {
            const auto string_id = (*road_edges)[index].value<std::string>();
            const auto edge = string_id.has_value() ? result.find_edge(*string_id) : std::nullopt;
            if (!edge.has_value()) {
                throw std::runtime_error{"civilization.toml road_edges 引用不存在"};
            }
            rules.road_edges[index] = *edge;
        }
        const auto* crossings = civilization["crossings"].as_array();
        if (crossings == nullptr || crossings->empty()) {
            throw std::runtime_error{"civilization.toml 缺少 [[crossings]]"};
        }
        for (const auto& node : *crossings) {
            const auto& table = require_table(node, civilization_path);
            const auto river_string = require_string(table, "river", civilization_path);
            const auto road_string = require_string(table, "road", civilization_path);
            const auto result_string = require_string(table, "result", civilization_path);
            const auto river = result.find_edge(river_string);
            const auto road = result.find_edge(road_string);
            const auto compound = result.find_edge(result_string);
            if (!river.has_value() || !road.has_value() || !compound.has_value()) {
                throw std::runtime_error{"civilization.toml crossing 引用不存在的 edge"};
            }
            rules.crossings.push_back({*river, *road, *compound});
        }
        std::set<std::pair<std::uint16_t, std::uint16_t>> crossing_keys;
        const std::array river_names{"edge.stream", "edge.river", "edge.great_river"};
        for (const auto road : rules.road_edges) {
            const auto* road_definition = result.edge(road);
            if (road_definition == nullptr ||
                (road_definition->flags & kEdgeRoadFlag) == 0) {
                throw std::runtime_error{"civilization.toml road_edges 不是道路 def"};
            }
        }
        for (const auto& crossing : rules.crossings) {
            const auto key = std::pair{value_of(crossing.river), value_of(crossing.road)};
            const auto* compound = result.edge(crossing.result);
            if (!crossing_keys.insert(key).second || compound == nullptr ||
                (compound->flags & (kEdgeRoadFlag | kEdgeRiverFlag | kEdgeBridgeFlag)) !=
                    (kEdgeRoadFlag | kEdgeRiverFlag | kEdgeBridgeFlag)) {
                throw std::runtime_error{"civilization.toml crossing 重複或不是複合 def"};
            }
        }
        for (const auto river_name : river_names) {
            const auto river = result.find_edge(river_name);
            if (!river.has_value()) {
                throw std::runtime_error{"civilization.toml 缺少標準河流 def"};
            }
            for (const auto road : rules.road_edges) {
                if (!crossing_keys.contains({value_of(*river), value_of(road)})) {
                    throw std::runtime_error{"civilization.toml 河級 × 道路級查表不完整"};
                }
            }
        }
        rules.loaded = true;
    }
    return result;
}

}  // namespace aetheria::rules
