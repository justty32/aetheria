// ruleset_load_site_city_detail.h：跨翻譯單元傳遞原始載入資料。

#pragma once

#include <filesystem>
#include <toml++/impl/forward_declarations.hpp>

namespace aetheria::rules::detail {

struct SiteCityLoadContext {
    const toml::array* styles;
    const toml::table* fortification;
    const std::filesystem::path& path;
};

}  // namespace aetheria::rules::detail
