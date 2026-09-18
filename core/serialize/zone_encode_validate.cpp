#include "core/serialize/zone_codec.h"

#include "core/serialize/all_components.h"
#include "core/serialize/registry_codec.h"
#include "core/serialize/zone_codec_detail.h"
#include "core/serialize/zone_diplomacy_codec.h"
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

#include "core/serialize/zone_encode_detail.h"

namespace aetheria::serialize::detail {

namespace {

template <typename Id>
void validate_ids(std::span<const Id> values, std::size_t definition_count,
                  std::string_view kind) {
    for (const auto value : values) {
        if (static_cast<std::size_t>(rules::value_of(value)) >= definition_count) {
            throw std::runtime_error{"zone " + std::string{kind} + " 下標超出目前 Ruleset"};
        }
    }
}

} // namespace

void validate_encode_zone(const zone::Zone& value, const rules::Ruleset& ruleset) {
    detail::validate_zone_meta(value);
    if (!zone::payload_matches_level(value.key, value.payload)) {
        throw std::runtime_error{"SpatialPayload alternative 與 ZoneKey level 不符"};
    }
    if (value.diplomacy.has_value() && value.key != zone::kRootZone) {
        throw std::runtime_error{"外交狀態只能存在 root zone"};
    }
    const auto city_states = value.reg.view<const site::CityBuildState>();
    if (city_states.size() > 1U ||
        (!city_states.empty() && zone::level_of(value.key) != zone::ZoneLevel::Site) ||
        (!city_states.empty() &&
         !site::valid_city_build_state(
             city_states.get<const site::CityBuildState>(*city_states.begin()), ruleset))) {
        throw std::runtime_error{"zone 含無效 CityBuildState"};
    }
    const auto digests = value.reg.view<const site::SiteDigest>();
    if (digests.size() > 1U ||
        (!digests.empty() && zone::level_of(value.key) != zone::ZoneLevel::Site) ||
        (!digests.empty() && !city_states.empty()) ||
        (!digests.empty() &&
         !site::valid_site_digest(digests.get<const site::SiteDigest>(*digests.begin()),
                                  ruleset))) {
        throw std::runtime_error{"zone 含無效 SiteDigest"};
    }
    const auto fate_ledgers = value.reg.view<const world::NamedFateLedger>();
    const auto level = zone::level_of(value.key);
    if (fate_ledgers.size() > 1U ||
        (!fate_ledgers.empty() && level != zone::ZoneLevel::Region &&
         level != zone::ZoneLevel::Site) ||
        (!fate_ledgers.empty() &&
         !world::valid_named_fate_ledger(
             fate_ledgers.get<const world::NamedFateLedger>(*fate_ledgers.begin())))) {
        throw std::runtime_error{"zone 含無效 NamedFateLedger"};
    }
    if (const auto* region = std::get_if<zone::RegionPayload>(&value.payload)) {
        for (const auto& [z, tiles] : region->layers) {
            static_cast<void>(z);
            if (!tiles.valid_layout()) {
                throw std::runtime_error{"zone RegionTiles 欄位尺寸不一致"};
            }
            validate_ids<rules::TerrainId>(tiles.base, ruleset.terrains().size(), "TerrainId");
            validate_ids<rules::ReliefId>(tiles.relief, ruleset.reliefs().size(), "ReliefId");
            validate_ids<rules::FeatureId>(tiles.feature, ruleset.features().size(), "FeatureId");
            validate_ids<rules::EdgeId>(tiles.edges, ruleset.edges().size(), "EdgeId");
            if (std::ranges::any_of(tiles.settlement, [](world::SettlementTier tier) {
                    return tier > world::SettlementTier::City;
                })) {
                throw std::runtime_error{"zone RegionTiles 含無效 SettlementTier"};
            }
        }
    } else if (const auto* site_payload = std::get_if<zone::SitePayload>(&value.payload)) {
        if (!site::valid_persistent_layer(site_payload->layers.persistent)) {
            throw std::runtime_error{"zone SitePersistentLayer 含無效建築資料"};
        }
    } else if (const auto* local_payload = std::get_if<zone::LocalPayload>(&value.payload)) {
        if (!local::valid_dungeon_persistent_state(local_payload->dungeon)) {
            throw std::runtime_error{"zone LocalPayload 含無效地城持久層"};
        }
    }
    const auto door_states = value.reg.view<const local::LocalDoorState>();
    if (door_states.size() > 1U ||
        (!door_states.empty() && zone::level_of(value.key) != zone::ZoneLevel::Local)) {
        throw std::runtime_error{"LocalDoorState 只能是 Local zone 的單例 component"};
    }
}

} // namespace aetheria::serialize::detail
