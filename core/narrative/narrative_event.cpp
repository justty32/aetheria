// narrative_event.cpp：命運判定呈現用的結構化假資料，不實作判定本身。

#include "core/narrative/narrative_event.h"

#include <utility>

namespace aetheria::narrative {
namespace {

[[nodiscard]] NamedArgument literal(std::string name, std::string value) {
    return {std::move(name), std::move(value), ArgumentKind::Literal};
}

[[nodiscard]] NamedArgument i18n(std::string name, std::string key) {
    return {std::move(name), std::move(key), ArgumentKind::I18nKey};
}

}  // namespace

EventFeed make_fate_presentation_fixture() {
    NarrativeEvent famine;
    famine.id = 1;
    famine.heading = {
        "event.fate.heading",
        {i18n("event", "event.kind.famine"), literal("xun", "3"),
         i18n("place", "place.stonebridge")},
    };
    famine.lines = {
        {"event.fate.cohort_loss",
         {i18n("cohort", "cohort.civilian"), literal("count", "1197"),
          literal("percent", "12")}},
        {"event.fate.named.property_lost",
         {i18n("person", "person.martha_grocer")}},
        {"event.fate.named.died",
         {i18n("person", "person.glen_veteran"), i18n("event", "event.kind.famine")}},
    };
    return EventFeed{{std::move(famine)}};
}

}  // namespace aetheria::narrative
