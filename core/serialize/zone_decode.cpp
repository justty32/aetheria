#include "core/serialize/zone_codec.h"

#include "core/serialize/all_components.h"
#include "core/serialize/registry_codec.h"
#include "core/serialize/zone_codec_detail.h"
#include "core/serialize/zone_diplomacy_codec.h"
#include "core/serialize/zone_region_portals.h"
#include "core/site/site_lifecycle.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/vector.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aetheria::serialize {
namespace {

template <typename Id, typename Finder>
[[nodiscard]] std::vector<Id> build_remap(const std::vector<std::string>& saved_ids,
                                          Finder&& find, std::string_view kind) {
    if (saved_ids.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) +
                               1U) {
        throw std::runtime_error{"zone 存檔的 " + std::string{kind} + " id 表超過 uint16 容量"};
    }
    std::vector<Id> result;
    result.reserve(saved_ids.size());
    for (const auto& string_id : saved_ids) {
        const auto current = find(string_id);
        if (!current.has_value()) {
            throw std::runtime_error{"zone 存檔含目前 Ruleset 不存在的 " + std::string{kind} +
                                     " id：" + string_id};
        }
        result.push_back(*current);
    }
    return result;
}

template <typename Id>
void remap_ids(std::vector<Id>& values, const std::vector<Id>& remap, std::string_view kind) {
    for (auto& value : values) {
        const auto old_index = static_cast<std::size_t>(rules::value_of(value));
        if (old_index >= remap.size()) {
            throw std::runtime_error{"zone " + std::string{kind} + " 下標超出存檔 id 表"};
        }
        value = remap[old_index];
    }
}

}  // namespace

std::unique_ptr<zone::Zone> decode_zone(std::string_view bytes, const rules::Ruleset& ruleset,
                                       ZoneDecodeMode mode) {
    std::istringstream stream{std::string{bytes}, std::ios::binary};
    std::uint64_t key{};
    std::int64_t saved_tick{};
    std::uint32_t version{};
    std::unique_ptr<zone::Zone> value;
    {
        cereal::PortableBinaryInputArchive archive{stream};
        std::uint32_t magic{};
        std::uint8_t persistence_flags{};
        archive(magic, version, key, saved_tick, persistence_flags);
        if (magic != detail::kZoneMagic) {
            throw std::runtime_error{"zone magic 不符"};
        }
        const bool is_legacy_fixture =
            mode == ZoneDecodeMode::LegacyFixture && (version == 14 || version == 15);
        if (version != kSaveFormatVersion && !is_legacy_fixture) {
            throw std::runtime_error{"zone format_version 不符：檔內=" + std::to_string(version) +
                                     " 預期=" + std::to_string(kSaveFormatVersion)};
        }
        if (persistence_flags != detail::kReservedPersistenceFlags) {
            throw std::runtime_error{"zone persistence flags 不支援"};
        }
        std::vector<std::string> terrain_ids;
        std::vector<std::string> relief_ids;
        std::vector<std::string> feature_ids;
        std::vector<std::string> edge_ids;
        archive(terrain_ids, relief_ids, feature_ids, edge_ids);
        const auto terrain_remap = build_remap<rules::TerrainId>(
            terrain_ids, [&](std::string_view id) { return ruleset.find_terrain(id); }, "terrain");
        const auto relief_remap = build_remap<rules::ReliefId>(
            relief_ids, [&](std::string_view id) { return ruleset.find_relief(id); }, "relief");
        const auto feature_remap = build_remap<rules::FeatureId>(
            feature_ids, [&](std::string_view id) { return ruleset.find_feature(id); }, "feature");
        const auto edge_remap = build_remap<rules::EdgeId>(
            edge_ids, [&](std::string_view id) { return ruleset.find_edge(id); }, "edge");
        std::uint8_t payload_index{};
        archive(payload_index);
        if (payload_index == 2 && version != kSaveFormatVersion) {
            throw std::runtime_error{"zone Site payload format_version 不符：檔內=" +
                                     std::to_string(version) + " 預期=" +
                                     std::to_string(kSaveFormatVersion)};
        }
        zone::SpatialPayload payload;
        switch (payload_index) {
        case 0:
            payload = std::monostate{};
            break;
        case 1: {
            zone::RegionPayload region;
            std::uint64_t layer_count{};
            archive(layer_count);
            if (layer_count > 256U) {
                throw std::runtime_error{"zone RegionPayload layer 數超過 int8_t 容量"};
            }
            for (std::uint64_t layer = 0; layer < layer_count; ++layer) {
                std::int8_t z{};
                world::RegionTiles tiles;
                std::vector<std::uint8_t> ever_realized;
                archive(z, tiles.width, tiles.height, tiles.base, tiles.relief, tiles.feature,
                        tiles.temperature, tiles.moisture, tiles.elevation, tiles.edges,
                        tiles.owner, tiles.settlement, tiles.defense, tiles.damage, ever_realized,
                        tiles.reduction_fields_.fields);
                detail::load_region_portals(archive, tiles);
                const auto count64 = static_cast<std::uint64_t>(tiles.width) * tiles.height;
                if (tiles.width == 0 || tiles.height == 0 ||
                    count64 > std::numeric_limits<std::size_t>::max() / 4U) {
                    throw std::runtime_error{"zone RegionTiles 尺寸超出可表達範圍"};
                }
                tiles.site.resize(static_cast<std::size_t>(count64));
                if (!tiles.valid_layout() || ever_realized.size() != tiles.tile_count()) {
                    throw std::runtime_error{"zone RegionTiles 欄位尺寸不一致"};
                }
                if (std::ranges::any_of(tiles.settlement, [](world::SettlementTier tier) {
                        return tier > world::SettlementTier::City;
                    })) {
                    throw std::runtime_error{"zone RegionTiles 含無效 SettlementTier"};
                }
                for (std::size_t index = 0; index < tiles.site.size(); ++index) {
                    if (ever_realized[index] > 1) {
                        throw std::runtime_error{"zone SiteState ever_realized 不是布林值"};
                    }
                    tiles.site[index].ever_realized = ever_realized[index] != 0;
                }
                remap_ids(tiles.base, terrain_remap, "TerrainId");
                remap_ids(tiles.relief, relief_remap, "ReliefId");
                remap_ids(tiles.feature, feature_remap, "FeatureId");
                remap_ids(tiles.edges, edge_remap, "EdgeId");
                if (!region.layers.emplace(z, std::move(tiles)).second) {
                    throw std::runtime_error{"zone RegionPayload 含重複 z layer"};
                }
            }
            payload = std::move(region);
            break;
        }
        case 2: {
            zone::SitePayload site_payload;
            archive_saved_site_layers(archive, site_payload.layers, SavedSiteLayers{});
            if (!site::valid_persistent_layer(site_payload.layers.persistent)) {
                throw std::runtime_error{"zone SitePersistentLayer 含無效建築資料"};
            }
            payload = std::move(site_payload);
            break;
        }
        case 3: {
            zone::LocalPayload local_payload;
            archive(local_payload.dungeon.triggered_trap_uids,
                    local_payload.dungeon.claimed_treasure_uids);
            if (!local::valid_dungeon_persistent_state(local_payload.dungeon)) {
                throw std::runtime_error{"zone LocalPayload 含無效地城持久層"};
            }
            payload = std::move(local_payload);
            break;
        }
        default:
            throw std::runtime_error{"zone SpatialPayload alternative tag 不支援"};
        }
        value = std::make_unique<zone::Zone>(zone::ZoneKey{key}, std::move(payload));
        value->last_saved_tick = time::Tick{saved_tick};
    }
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
    if (stream.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error{"zone 檔含未解析的尾端資料"};
    }
    detail::validate_zone_meta(*value);
    return value;
}

}  // namespace aetheria::serialize
