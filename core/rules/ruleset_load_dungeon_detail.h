// ruleset_load_dungeon_detail.h：跨翻譯單元傳遞原始載入資料。

#pragma once

#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <toml++/impl/forward_declarations.hpp>

namespace aetheria::rules::detail {

struct DungeonTrapLoadContext {
    const toml::array* traps;
    const std::filesystem::path& path;
    std::set<std::string, std::less<>>& global_ids;
};

}  // namespace aetheria::rules::detail
