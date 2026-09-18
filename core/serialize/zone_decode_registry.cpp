#include "core/serialize/zone_codec.h"

#include "core/serialize/all_components.h"
#include "core/serialize/registry_codec.h"
#include "core/serialize/zone_codec_detail.h"
#include "core/serialize/zone_diplomacy_codec.h"
#include "core/serialize/zone_region_portals.h"
#include "core/site/site_lifecycle.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/vector.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "core/serialize/zone_decode_detail.h"

namespace aetheria::serialize::detail {

void decode_registry(std::istream& stream, zone::Zone* value,
                     const rules::Ruleset& ruleset, std::uint32_t version) {
    value->reg.clear();
    {
        cereal::PortableBinaryInputArchive registry_cereal{stream};
        RegistryInputArchive registry_archive{registry_cereal};
        if (version == kSaveFormatVersion) {
            load_registry_snapshot(value->reg, registry_archive, AllComponents{});
        } else {
            load_registry_snapshot(value->reg, registry_archive, AllComponentsV15{});
        }
    }
    if (version >= 15) {
        cereal::PortableBinaryInputArchive diplomacy_archive{stream};
        value->diplomacy = detail::load_diplomacy(diplomacy_archive, ruleset, version);
        if (value->diplomacy.has_value() && value->key != zone::kRootZone) {
            throw std::runtime_error{"外交狀態只能存在 root zone"};
        }
    } else {
        value->diplomacy.reset();
    }
    const auto city_states = value->reg.view<const site::CityBuildState>();
    if (city_states.size() > 1U ||
        (!city_states.empty() && zone::level_of(value->key) != zone::ZoneLevel::Site) ||
        (!city_states.empty() &&
         !site::valid_city_build_state(
             city_states.get<const site::CityBuildState>(*city_states.begin()), ruleset))) {
        throw std::runtime_error{"zone 含無效 CityBuildState"};
    }
    const auto digests = value->reg.view<const site::SiteDigest>();
    if (digests.size() > 1U ||
        (!digests.empty() && zone::level_of(value->key) != zone::ZoneLevel::Site) ||
        (!digests.empty() && !city_states.empty()) ||
        (!digests.empty() &&
         !site::valid_site_digest(digests.get<const site::SiteDigest>(*digests.begin()),
                                  ruleset))) {
        throw std::runtime_error{"zone 含無效 SiteDigest"};
    }
    const auto fate_ledgers = value->reg.view<const world::NamedFateLedger>();
    const auto level = zone::level_of(value->key);
    if (fate_ledgers.size() > 1U ||
        (!fate_ledgers.empty() && level != zone::ZoneLevel::Region &&
         level != zone::ZoneLevel::Site) ||
        (!fate_ledgers.empty() &&
         !world::valid_named_fate_ledger(
             fate_ledgers.get<const world::NamedFateLedger>(*fate_ledgers.begin())))) {
        throw std::runtime_error{"zone 含無效 NamedFateLedger"};
    }
    const auto door_states = value->reg.view<const local::LocalDoorState>();
    if (door_states.size() > 1U ||
        (!door_states.empty() && zone::level_of(value->key) != zone::ZoneLevel::Local)) {
        throw std::runtime_error{"LocalDoorState 只能是 Local zone 的單例 component"};
    }
}

} // namespace aetheria::serialize::detail
