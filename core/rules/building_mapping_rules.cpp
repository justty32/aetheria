// building_mapping_rules.cpp：Ruleset 持久建築型別的穩定文字映射。

#include "core/rules/building_mapping_rules.h"

namespace aetheria::rules {

std::optional<PersistentBuildingType>
persistent_building_type_from_string(std::string_view value) noexcept {
    if (value == "settlement_hall") {
        return PersistentBuildingType::SettlementHall;
    }
    if (value == "residence") {
        return PersistentBuildingType::Residence;
    }
    if (value == "farm") {
        return PersistentBuildingType::Farm;
    }
    if (value == "mine") {
        return PersistentBuildingType::Mine;
    }
    if (value == "workshop") {
        return PersistentBuildingType::Workshop;
    }
    if (value == "civic_square") {
        return PersistentBuildingType::CivicSquare;
    }
    return std::nullopt;
}

std::string_view persistent_building_type_name(PersistentBuildingType value) noexcept {
    switch (value) {
    case PersistentBuildingType::SettlementHall:
        return "settlement_hall";
    case PersistentBuildingType::Residence:
        return "residence";
    case PersistentBuildingType::Farm:
        return "farm";
    case PersistentBuildingType::Mine:
        return "mine";
    case PersistentBuildingType::Workshop:
        return "workshop";
    case PersistentBuildingType::CivicSquare:
        return "civic_square";
    }
    return {};
}

}  // namespace aetheria::rules
