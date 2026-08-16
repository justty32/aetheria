#pragma once

#include "core/rules/ruleset.h"

namespace aetheria::tests {

[[nodiscard]] inline const rules::Ruleset& test_ruleset() {
    static const auto value = rules::RulesetLoader::load(AETHERIA_SOURCE_DIR "/data");
    return value;
}

}  // namespace aetheria::tests
