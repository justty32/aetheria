#pragma once

#include "core/rules/ruleset.h"

#include <cstdint>
#include <string>

namespace aetheria::sim::detail {

int run_core_probe(const rules::Ruleset& ruleset, std::int64_t requested_tick,
                   const std::string& save_directory);

} // namespace aetheria::sim::detail
