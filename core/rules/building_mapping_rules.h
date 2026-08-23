#pragma once

// building_mapping_rules.h 定義城建 def 映射到持久建築型別的 Ruleset 詞彙。

#include <cstdint>
#include <optional>
#include <string_view>

namespace aetheria::rules {

enum class PersistentBuildingType : std::uint8_t {
    SettlementHall,
    Residence,
    Farm,
    Mine,
    Workshop,
    CivicSquare,
};

[[nodiscard]] std::optional<PersistentBuildingType>
persistent_building_type_from_string(std::string_view value) noexcept;
[[nodiscard]] std::string_view
persistent_building_type_name(PersistentBuildingType value) noexcept;

}  // namespace aetheria::rules
