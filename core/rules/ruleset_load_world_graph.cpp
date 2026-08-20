// core/rules/ruleset_load_world_graph.cpp：手工 WorldGraph 通道宣告的載入與驗證。

#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

namespace aetheria::rules {
namespace {

[[nodiscard]] WorldConnectionType connection_type(std::string_view value,
                                                   const std::filesystem::path& path) {
    if (value == "sea_route") {
        return WorldConnectionType::SeaRoute;
    }
    if (value == "mountain_pass") {
        return WorldConnectionType::MountainPass;
    }
    if (value == "underground") {
        return WorldConnectionType::Underground;
    }
    if (value == "teleport") {
        return WorldConnectionType::Teleport;
    }
    throw std::runtime_error{"world_graph.toml 通道 type 無效：" + std::string{value} +
                             "：" + path.string()};
}

[[nodiscard]] std::optional<WorldConnectionEndpoint>
optional_coordinate(const toml::table& table, std::string_view field,
                    const std::filesystem::path& path) {
    const auto* values = table[field].as_array();
    if (values == nullptr) {
        return std::nullopt;
    }
    if (values->size() != 2U) {
        throw std::runtime_error{"world_graph.toml 座標必須是 [x, y]：" +
                                 std::string{field}};
    }
    const auto x = (*values)[0].value<std::int64_t>();
    const auto y = (*values)[1].value<std::int64_t>();
    if (!x.has_value() || !y.has_value() || *x < 0 || *y < 0 ||
        *x > std::numeric_limits<std::int16_t>::max() ||
        *y > std::numeric_limits<std::int16_t>::max()) {
        throw std::runtime_error{"world_graph.toml 座標超出非負 int16：" + path.string()};
    }
    return WorldConnectionEndpoint{static_cast<std::int16_t>(*x),
                                   static_cast<std::int16_t>(*y)};
}

}  // namespace

void RulesetLoader::load_world_graph(Ruleset& result,
                                     const std::filesystem::path& data_directory) {
    const auto path = data_directory / "world_graph.toml";
    if (!std::filesystem::is_regular_file(path)) {
        return;
    }
    toml::table document;
    try {
        document = toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() + "：" +
                                 std::string{error.description()}};
    }
    const auto* regions = document["regions"].as_array();
    const auto* connections = document["connections"].as_array();
    if (regions == nullptr || regions->empty() || connections == nullptr) {
        throw std::runtime_error{"world_graph.toml 必須有非空 regions 與 [[connections]]"};
    }
    std::set<std::uint32_t> region_ids;
    for (const auto& node : *regions) {
        const auto value = node.value<std::int64_t>();
        if (!value.has_value() || *value < 0 || *value > UINT32_MAX ||
            !region_ids.insert(static_cast<std::uint32_t>(*value)).second) {
            throw std::runtime_error{"world_graph.toml regions 含無效或重複 id"};
        }
    }
    for (const auto& node : *connections) {
        const auto& table = detail::require_table(node, path);
        const auto raw_id = detail::require_integer(table, "id", path);
        const auto raw_a = detail::require_integer(table, "region_a", path);
        const auto raw_b = detail::require_integer(table, "region_b", path);
        const auto raw_cost = detail::require_integer(table, "cost_ticks", path);
        if (raw_id <= 0 || raw_id > UINT32_MAX || raw_a < 0 || raw_a > UINT32_MAX ||
            raw_b < 0 || raw_b > UINT32_MAX || raw_a == raw_b || raw_cost <= 0 ||
            raw_cost > UINT32_MAX) {
            throw std::runtime_error{"world_graph.toml 通道數值欄位無效"};
        }
        const auto region_a = static_cast<std::uint32_t>(raw_a);
        const auto region_b = static_cast<std::uint32_t>(raw_b);
        if (!region_ids.contains(region_a) || !region_ids.contains(region_b)) {
            throw std::runtime_error{"world_graph.toml 通道引用未宣告 Region"};
        }
        WorldGraphConnection connection{
            static_cast<WorldConnectionId>(raw_id), region_a, region_b,
            connection_type(detail::require_string(table, "type", path), path),
            static_cast<std::uint32_t>(raw_cost),
            detail::require_string(table, "requirement", path),
            optional_coordinate(table, "coordinate_a", path),
            optional_coordinate(table, "coordinate_b", path)};
        if (connection.type != WorldConnectionType::Teleport &&
            (connection.coordinate_a.has_value() || connection.coordinate_b.has_value())) {
            throw std::runtime_error{"只有 teleport 通道可在資料中指定座標"};
        }
        result.world_connections_.push_back(std::move(connection));
    }
    std::ranges::sort(result.world_connections_, {}, [](const auto& connection) {
        return value_of(connection.id);
    });
    if (std::ranges::adjacent_find(result.world_connections_, {}, [](const auto& connection) {
            return value_of(connection.id);
        }) != result.world_connections_.end()) {
        throw std::runtime_error{"world_graph.toml 通道 id 重複"};
    }
}

}  // namespace aetheria::rules
