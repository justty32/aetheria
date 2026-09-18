// ruleset_load_power_sources_detail.h：跨翻譯單元傳遞原始載入資料。

#pragma once

#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <toml++/impl/forward_declarations.hpp>

namespace aetheria::rules::detail {

struct PowerSourceLoadContext {
    const toml::table& document;
    const std::filesystem::path& path;
    std::set<std::string, std::less<>>& global_ids;
};

}  // namespace aetheria::rules::detail
