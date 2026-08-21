#pragma once

// site_viewer.h 提供 `aetheria_sim gen site` 的城市／荒野 PNG 檢視入口。

#include "core/rules/ruleset.h"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace aetheria::sim {

int run_gen_site(const rules::Ruleset& ruleset, std::uint64_t site_seed, std::string_view kind,
                 const std::filesystem::path& output_directory);

} // namespace aetheria::sim
