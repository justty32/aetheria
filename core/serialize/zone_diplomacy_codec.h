#pragma once

// zone v16 的外交持久區塊；v15 沒有真值、情報快取與 AI 目標，載入時保持缺席。

#include "core/rules/ruleset.h"
#include "core/world/diplomacy.h"

#include <cereal/archives/portable_binary.hpp>

#include <optional>

namespace aetheria::serialize::detail {

void save_diplomacy(cereal::PortableBinaryOutputArchive& archive,
                    const std::optional<world::WorldDiplomacyState>& diplomacy,
                    const rules::Ruleset& ruleset);

[[nodiscard]] std::optional<world::WorldDiplomacyState>
load_diplomacy(cereal::PortableBinaryInputArchive& archive, const rules::Ruleset& ruleset,
               std::uint32_t version);

} // namespace aetheria::serialize::detail
