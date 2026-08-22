#pragma once

// zone v15 的外交持久區塊；def 一律寫字串 id，載入時重映射目前 Ruleset。

#include "core/rules/ruleset.h"
#include "core/world/diplomacy.h"

#include <cereal/archives/portable_binary.hpp>

#include <optional>

namespace aetheria::serialize::detail {

void save_diplomacy(cereal::PortableBinaryOutputArchive& archive,
                    const std::optional<world::WorldDiplomacyState>& diplomacy,
                    const rules::Ruleset& ruleset);

[[nodiscard]] std::optional<world::WorldDiplomacyState>
load_diplomacy(cereal::PortableBinaryInputArchive& archive, const rules::Ruleset& ruleset);

} // namespace aetheria::serialize::detail
