#include "core/serialize/zone_codec.h"

#include "core/serialize/all_components.h"
#include "core/serialize/registry_codec.h"
#include "core/serialize/zone_codec_detail.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>
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

constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

template <typename Def>
[[nodiscard]] std::vector<std::string> string_ids(std::span<const Def> defs) {
    std::vector<std::string> result;
    result.reserve(defs.size());
    for (const auto& def : defs) {
        result.push_back(def.id);
    }
    return result;
}

template <typename Id>
void validate_ids(std::span<const Id> values, std::size_t definition_count,
                  std::string_view kind) {
    for (const auto value : values) {
        if (static_cast<std::size_t>(rules::value_of(value)) >= definition_count) {
            throw std::runtime_error{"zone " + std::string{kind} + " 下標超出目前 Ruleset"};
        }
    }
}

}  // namespace

std::string encode_zone(const zone::Zone& value, const rules::Ruleset& ruleset) {
    detail::validate_zone_meta(value);
    if (!zone::payload_matches_level(value.key, value.payload)) {
        throw std::runtime_error{"SpatialPayload alternative 與 ZoneKey level 不符"};
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
    }
    std::ostringstream stream{std::ios::binary};
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        const auto key = zone::value_of(value.key);
        const auto saved_tick = static_cast<std::int64_t>(value.last_saved_tick);
        archive(detail::kZoneMagic, kSaveFormatVersion, key, saved_tick,
                detail::kReservedPersistenceFlags);
        auto terrain_ids = string_ids(ruleset.terrains());
        auto relief_ids = string_ids(ruleset.reliefs());
        auto feature_ids = string_ids(ruleset.features());
        auto edge_ids = string_ids(ruleset.edges());
        archive(terrain_ids, relief_ids, feature_ids, edge_ids);
        const auto payload_index = static_cast<std::uint8_t>(value.payload.index());
        archive(payload_index);
        if (const auto* region = std::get_if<zone::RegionPayload>(&value.payload)) {
            const auto layer_count = static_cast<std::uint64_t>(region->layers.size());
            archive(layer_count);
            for (const auto& [z, tiles] : region->layers) {
                std::vector<std::uint8_t> ever_realized;
                ever_realized.reserve(tiles.site.size());
                for (const auto& site : tiles.site) {
                    ever_realized.push_back(site.ever_realized ? UINT8_C(1) : UINT8_C(0));
                }
                archive(z, tiles.width, tiles.height, tiles.base, tiles.relief, tiles.feature,
                        tiles.temperature, tiles.moisture, tiles.elevation, tiles.edges,
                        tiles.owner, tiles.settlement, ever_realized);
                const auto portal_count = static_cast<std::uint64_t>(tiles.portals.size());
                archive(portal_count);
                for (const auto& portal : tiles.portals) {
                    archive(portal.tile.x, portal.tile.y, rules::value_of(portal.channel));
                }
            }
        } else if (const auto* site_payload = std::get_if<zone::SitePayload>(&value.payload)) {
            archive_saved_site_layers(archive, site_payload->layers, SavedSiteLayers{});
        }
    }
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        RegistryOutputArchive registry_archive{archive};
        save_registry_snapshot(value.reg, registry_archive, AllComponents{});
    }
    if (!stream) {
        throw std::runtime_error{"zone 序列化失敗"};
    }
    return std::move(stream).str();
}

std::uint64_t persistent_state_hash(const zone::Zone& value, const rules::Ruleset& ruleset) {
    const auto bytes = encode_zone(value, ruleset);
    auto hash = kFnvOffset;
    for (const auto byte : bytes) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= kFnvPrime;
    }
    return hash;
}

}  // namespace aetheria::serialize
