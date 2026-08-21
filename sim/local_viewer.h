#pragma once

// local_viewer.h 提供 `aetheria_sim gen local` 的既有結果 PNG 檢視入口。

#include "core/rules/ruleset.h"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace aetheria::sim {

int run_gen_local(const rules::Ruleset& ruleset, std::uint64_t site_seed, std::string_view zoning,
                  std::string_view z, const std::filesystem::path& output_directory);

} // namespace aetheria::sim
