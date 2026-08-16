#pragma once

// core/rules/toml_read.h：RulesetLoader 各分段共用的 TOML 讀取／def 註冊 helper。

#include "core/rules/ruleset.h"

#include <toml++/toml.hpp>

#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aetheria::rules::detail {

[[nodiscard]] inline const toml::array& read_array(const std::filesystem::path& path,
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

[[nodiscard]] inline const toml::array& read_defs(const std::filesystem::path& path) {
    return read_array(path, "defs");
}

[[nodiscard]] inline const toml::table& require_table(const toml::node& node,
                                                       const std::filesystem::path& path) {
    const auto* table = node.as_table();
    if (table == nullptr) {
        throw std::runtime_error{"Ruleset defs 項目不是 table：" + path.string()};
    }
    return *table;
}

[[nodiscard]] inline std::string require_string(const toml::table& table, std::string_view field,
                                                const std::filesystem::path& path) {
    const auto value = table[field].value<std::string>();
    if (!value.has_value() || value->empty()) {
        throw std::runtime_error{"Ruleset 缺少非空字串欄位 " + std::string{field} + "：" +
                                 path.string()};
    }
    return *value;
}

[[nodiscard]] inline std::int64_t require_integer(const toml::table& table,
                                                   std::string_view field,
                                                   const std::filesystem::path& path) {
    const auto value = table[field].value<std::int64_t>();
    if (!value.has_value()) {
        throw std::runtime_error{"Ruleset 缺少整數欄位 " + std::string{field} + "：" +
                                 path.string()};
    }
    return *value;
}

[[nodiscard]] inline std::int32_t require_int32(const toml::table& table, std::string_view field,
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

inline void register_global_id(std::set<std::string, std::less<>>& ids, const std::string& id,
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

}  // namespace aetheria::rules::detail
