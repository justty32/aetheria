#include "core/serialize/zone_codec.h"
#include "core/serialize/zone_decode_detail.h"

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

namespace aetheria::serialize {
namespace {

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
        const auto [terrain_remap, relief_remap, feature_remap, edge_remap] =
            detail::decode_definition_remaps(archive, ruleset);
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
    detail::decode_registry(stream, value.get(), ruleset, version);
    if (stream.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error{"zone 檔含未解析的尾端資料"};
    }
    detail::validate_zone_meta(*value);
    return value;
}

}  // namespace aetheria::serialize
