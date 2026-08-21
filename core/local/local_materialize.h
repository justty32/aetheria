#pragma once

// local_materialize.h：Site tile→Local 首次展開、純冷載與冷載後重算三個不同入口。

#include "core/local/local_tiles.h"
#include "core/zone/zone_manager.h"

#include <cstdint>
#include <optional>

namespace aetheria::local {

[[nodiscard]] zone::Zone materialize_local_zone(
    zone::ZoneKey site_key, const site::SiteProceduralLayer& parent, site::SiteXY coordinate,
    std::uint64_t site_seed, rules::FeatureId feature, const rules::Ruleset& ruleset);

// load_local_zone 只解碼持久狀態；本里程碑沒有 Local 持久欄位，程序 tiles 保持空白。
[[nodiscard]] std::optional<zone::ZoneHandle> load_local_zone(zone::ZoneManager& manager,
                                                             zone::ZoneKey local_key);

// rematerialize_local_zone 從 store 冷載後，以當下 Site 慢變數重算程序 tiles。
[[nodiscard]] zone::ZoneHandle rematerialize_local_zone(
    zone::ZoneManager& manager, zone::ZoneKey site_key,
    const site::SiteProceduralLayer& parent, site::SiteXY coordinate, std::uint64_t site_seed,
    rules::FeatureId feature, const rules::Ruleset& ruleset);

}  // namespace aetheria::local
