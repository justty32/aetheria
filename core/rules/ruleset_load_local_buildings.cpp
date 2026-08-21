// ruleset_load_local_buildings.cpp：Local 路線 A 幾何、邊與家具資料載入。

#include <limits>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>

#include "core/rules/ruleset.h"
#include "core/rules/toml_read.h"

namespace aetheria::rules {
namespace {

[[nodiscard]] LocalRoomKind read_room(std::string_view value, const std::filesystem::path& path) {
    if (value == "bedroom") {
        return LocalRoomKind::Bedroom;
    }
    if (value == "kitchen") {
        return LocalRoomKind::Kitchen;
    }
    if (value == "workshop") {
        return LocalRoomKind::Workshop;
    }
    if (value == "shop") {
        return LocalRoomKind::Shop;
    }
    throw std::runtime_error{"local_buildings.toml 含未支援房間類型：" + std::string{value} + "（" +
                             path.string() + "）"};
}

}  // namespace

using namespace detail;

void RulesetLoader::load_local_buildings(Ruleset& result,
                                         const std::filesystem::path& data_directory,
                                         std::set<std::string, std::less<>>& global_ids) {
    const auto path = data_directory / "local_buildings.toml";
    toml::table document;
    try {
        document = toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() + "：" +
                                 std::string{error.description()}};
    }
    const auto* building = document["building"].as_table();
    const auto* furniture = document["furniture"].as_array();
    if (building == nullptr || furniture == nullptr || furniture->empty()) {
        throw std::runtime_error{"local_buildings.toml 缺少 building 或 furniture"};
    }
    auto read = [&](std::string_view field, std::int64_t minimum,
                    std::int64_t maximum) -> std::int64_t {
        const auto value = require_integer(*building, field, path);
        if (value < minimum || value > maximum) {
            throw std::runtime_error{"local_buildings.toml 參數無效：" + std::string{field}};
        }
        return value;
    };
    auto read_ground = [&](std::string_view field) {
        const auto id = require_string(*building, field, path);
        const auto found = result.find_ground(id);
        if (!found.has_value()) {
            throw std::runtime_error{"local_buildings.toml 引用不存在的 GroundDef：" + id};
        }
        return *found;
    };
    auto read_edge = [&](std::string_view field, std::uint32_t flags) {
        const auto id = require_string(*building, field, path);
        const auto found = result.find_edge(id);
        if (!found.has_value() || (result.edge(*found)->flags & flags) != flags) {
            throw std::runtime_error{"local_buildings.toml 引用不存在或旗標不符的 EdgeDef：" + id};
        }
        return *found;
    };

    auto& rules = result.local_building_rules_;
    rules.house_margin = static_cast<std::uint8_t>(read("house_margin", 1, 8));
    rules.house_depth = static_cast<std::uint8_t>(read("house_depth", 10, 24));
    rules.house_frontage_min = static_cast<std::uint8_t>(read("house_frontage_min", 5, 24));
    rules.house_frontage_max = static_cast<std::uint8_t>(read("house_frontage_max", 5, 24));
    rules.room_split_depth = static_cast<std::uint8_t>(read("room_split_depth", 1, 4));
    rules.room_cut_min_percent = static_cast<std::uint8_t>(read("room_cut_min_percent", 1, 49));
    rules.room_cut_max_percent = static_cast<std::uint8_t>(read("room_cut_max_percent", 1, 49));
    rules.room_min_extent = static_cast<std::uint8_t>(read("room_min_extent", 5, 12));
    rules.upper_floor_percent = static_cast<std::uint8_t>(read("upper_floor_percent", 0, 100));
    rules.cellar_percent = static_cast<std::uint8_t>(read("cellar_percent", 0, 100));
    rules.residents_min = static_cast<std::uint8_t>(read("residents_min", 1, 32));
    rules.residents_max = static_cast<std::uint8_t>(read("residents_max", 1, 32));
    rules.foundation_ground = read_ground("foundation_ground");
    rules.wall_edge = read_edge("wall_edge", kEdgeWallFlag);
    constexpr auto door_flags = kEdgeWallFlag | kEdgeGateFlag | kEdgeOpenableFlag;
    rules.residential_door_edge = read_edge("residential_door_edge", door_flags);
    rules.commercial_door_edge = read_edge("commercial_door_edge", door_flags);
    rules.window_edge = read_edge("window_edge", kEdgeWallFlag | kEdgeWindowFlag);
    if (rules.house_frontage_min > rules.house_frontage_max ||
        rules.room_cut_min_percent > rules.room_cut_max_percent ||
        rules.residents_min > rules.residents_max ||
        rules.house_margin * 2U + rules.house_depth * 2U >= 64U ||
        rules.house_frontage_min < rules.room_min_extent) {
        throw std::runtime_error{"local_buildings.toml 幾何範圍無效"};
    }
    const auto outer_extent = static_cast<std::uint16_t>(64U - rules.house_margin * 2U);
    const auto middle_extent =
        static_cast<std::uint16_t>(64U - 2U * (rules.house_margin + rules.house_depth));
    const auto can_split_frontage = [&](std::uint16_t extent) {
        return (extent + rules.house_frontage_max - 1U) / rules.house_frontage_max <=
               extent / rules.house_frontage_min;
    };
    if (!can_split_frontage(outer_extent) || !can_split_frontage(middle_extent)) {
        throw std::runtime_error{"local_buildings.toml 幾何範圍無效"};
    }

    bool can_generate_furniture{};
    for (const auto& node : *furniture) {
        const auto& table = require_table(node, path);
        FurnitureDef def;
        def.id = require_string(table, "id", path);
        register_global_id(global_ids, def.id, "furniture.");
        def.room = read_room(require_string(table, "room", path), path);
        const auto minimum = require_integer(table, "minimum", path);
        const auto maximum = require_integer(table, "maximum", path);
        if (minimum < 0 || maximum < minimum || maximum > 8) {
            throw std::runtime_error{"local_buildings.toml 家具數量範圍無效：" + def.id};
        }
        def.minimum = static_cast<std::uint8_t>(minimum);
        def.maximum = static_cast<std::uint8_t>(maximum);
        can_generate_furniture = can_generate_furniture || def.maximum != 0;
        const auto id = append_def<FurnitureDefId>(result.furniture_, std::move(def));
        result.furniture_index_.emplace(result.furniture_.back().id, id);
    }
    if (!can_generate_furniture) {
        throw std::runtime_error{"local_buildings.toml 的家具表不會生成任何家具"};
    }
    rules.loaded = true;
}

}  // namespace aetheria::rules
