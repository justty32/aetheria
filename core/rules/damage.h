#pragma once

// core/rules/damage.h：定義資料驅動傷害型別、稀疏抗性項目，以及永遠不會
// 由抗性路徑產生完全免疫的整數傷害運算。

#include <cstdint>
#include <span>
#include <string>

namespace aetheria::rules {

enum class DamageTypeId : std::uint16_t {};

[[nodiscard]] constexpr std::uint16_t value_of(DamageTypeId id) noexcept {
    return static_cast<std::uint16_t>(id);
}

struct DamageTypeDef {
    std::string id;
    std::string name_key;
    std::string category;
};

struct DamageRules {
    std::int32_t max_resistance_percent{90};
};

struct DamageResistance {
    DamageTypeId type{};
    std::int32_t percent{};
};

struct DamageResult {
    std::int32_t base_damage{};
    std::int32_t requested_resistance_percent{};
    std::int32_t applied_resistance_percent{};
    std::int64_t actual_damage{};
};

[[nodiscard]] DamageResult apply_resistance(std::int32_t base_damage, DamageTypeId type,
                                            std::span<const DamageResistance> resistances,
                                            const DamageRules& rules);

}  // namespace aetheria::rules
