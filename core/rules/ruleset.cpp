#include "core/rules/ruleset.h"

#include <toml++/toml.hpp>

#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace aetheria::rules {
namespace {

[[nodiscard]] const toml::array& read_defs(const std::filesystem::path& path) {
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
    const auto* defs = document["defs"].as_array();
    if (defs == nullptr) {
        throw std::runtime_error{"Ruleset 缺少 [[defs]] 區段：" + path.string()};
    }
    return *defs;
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

template <typename Def>
void read_common(const toml::table& table, const std::filesystem::path& path, Def& def) {
    def.id = require_string(table, "id", path);
    def.name_key = require_string(table, "name_key", path);
    const auto move_cost = require_integer(table, "move_cost", path);
    if (move_cost < 1 || move_cost > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error{"Ruleset move_cost 必須 >= 1：" + def.id};
    }
    def.move_cost = static_cast<std::int32_t>(move_cost);
    const auto flags = require_integer(table, "flags", path);
    if (flags < 0 || static_cast<std::uint64_t>(flags) >
                         std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error{"Ruleset flags 超出 uint32：" + def.id};
    }
    def.flags = static_cast<std::uint32_t>(flags);
    def.visual.key = require_string(table, "visual", path);
}

void register_global_id(std::set<std::string, std::less<>>& ids, const std::string& id,
                        std::string_view required_prefix) {
    if (!id.starts_with(required_prefix)) {
        throw std::runtime_error{"Ruleset id 缺少類型前綴 " + std::string{required_prefix} +
                                 "：" + id};
    }
    if (!ids.insert(id).second) {
        throw std::runtime_error{"Ruleset 全域 id 重複：" + id};
    }
}

template <typename Id, typename Def>
[[nodiscard]] Id append_def(std::vector<Def>& defs, Def def) {
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
        read_common(table, feature_path, def);
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
        read_common(table, edge_path, def);
        register_global_id(global_ids, def.id, "edge.");
        const auto id = append_def<EdgeId>(result.edges_, std::move(def));
        result.edge_index_.emplace(result.edges_.back().id, id);
    }

    for (const auto& [feature_index, terrain_string_id] : feature_terrain_references) {
        const auto terrain = result.find_terrain(terrain_string_id);
        if (!terrain.has_value()) {
            throw std::runtime_error{"FeatureDef 引用不存在的 terrain id：" +
                                     terrain_string_id};
        }
        result.features_.at(feature_index).required_terrain = *terrain;
    }
    return result;
}

}  // namespace aetheria::rules
