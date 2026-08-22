#pragma once

// 外交資料定義：關係回歸、條約、宣戰理由、厭戰與和談門檻。
// Ruleset 擁有所有字串與值，外交世界狀態只借用不可變規則。

#include <cstdint>
#include <string>
#include <vector>

#include <aetheria/diplomacy/peace.h>

namespace aetheria::rules {

enum class TreatyDefId : std::uint16_t {};
enum class CasusBelliDefId : std::uint16_t {};

[[nodiscard]] constexpr std::uint16_t value_of(TreatyDefId id) noexcept {
    return static_cast<std::uint16_t>(id);
}
[[nodiscard]] constexpr std::uint16_t value_of(CasusBelliDefId id) noexcept {
    return static_cast<std::uint16_t>(id);
}

struct RelationReversionRates {
    std::uint16_t denominator{};
    std::uint16_t favor{};
    std::uint16_t trust{};
    std::uint16_t fear{};
    std::uint16_t grievance{};
};

struct TreatyBreachConsequence {
    std::int32_t favor{};
    std::int32_t trust{};
    std::int32_t grievance{};
};

struct TreatyDef {
    std::string id;
    std::uint32_t duration_xun{};
    std::string condition;
    TreatyBreachConsequence breach;
    bool renewable{};
};

struct CasusBelliDef {
    std::string id;
    std::uint32_t duration_xun{};
    std::string condition;
};

struct WarWearinessRules {
    std::int32_t base_per_xun{};
    std::int32_t per_thousand_casualties{};
    std::int32_t peace_threshold{};
};

struct DiplomacyRules {
    RelationReversionRates reversion;
    std::int32_t relation_min{};
    std::int32_t relation_max{};
    std::int32_t unjustified_war_trust_penalty{};
    WarWearinessRules war_weariness;
    diplomacy::PeaceLeverageWeights peace_weights;
    diplomacy::PeaceTermThresholds peace_thresholds;
    std::vector<TreatyDef> treaties;
    std::vector<CasusBelliDef> casus_belli;
    bool loaded{};
};

} // namespace aetheria::rules
