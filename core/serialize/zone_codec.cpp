#include "core/serialize/zone_codec.h"

#include "core/serialize/all_components.h"
#include "core/serialize/registry_codec.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <cstddef>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aetheria::serialize {
namespace {

constexpr std::uint32_t kZoneMagic = UINT32_C(0x415A4F4E);
constexpr std::uint8_t kReservedPersistenceFlags = 0;
constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

void validate_zone_meta(const zone::Zone& value) {
    const auto meta = value.reg.view<const zone::ZoneMeta>();
    if (meta.empty()) {
        throw std::runtime_error{"zone registry 缺少 ZoneMeta placeholder"};
    }
    for (const auto entity : meta) {
        if (meta.get<zone::ZoneMeta>(entity).zone_key != zone::value_of(value.key)) {
            throw std::runtime_error{"ZoneMeta 的 zone_key 與 zone 檔頭不符"};
        }
    }
}

template <typename Def>
[[nodiscard]] std::vector<std::string> string_ids(std::span<const Def> defs) {
    std::vector<std::string> result;
    result.reserve(defs.size());
    for (const auto& def : defs) {
        result.push_back(def.id);
    }
    return result;
}

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
    validate_zone_meta(value);
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
        }
    }
    std::ostringstream stream{std::ios::binary};
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        const auto key = zone::value_of(value.key);
        const auto saved_tick = static_cast<std::int64_t>(value.last_saved_tick);
        archive(kZoneMagic, kSaveFormatVersion, key, saved_tick, kReservedPersistenceFlags);
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
                        tiles.owner, ever_realized);
            }
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

std::unique_ptr<zone::Zone> decode_zone(std::string_view bytes, const rules::Ruleset& ruleset) {
    std::istringstream stream{std::string{bytes}, std::ios::binary};
    std::uint64_t key{};
    std::int64_t saved_tick{};
    std::unique_ptr<zone::Zone> value;
    {
        cereal::PortableBinaryInputArchive archive{stream};
        std::uint32_t magic{};
        std::uint32_t version{};
        std::uint8_t persistence_flags{};
        archive(magic, version, key, saved_tick, persistence_flags);
        if (magic != kZoneMagic) {
            throw std::runtime_error{"zone magic 不符"};
        }
        if (version != kSaveFormatVersion) {
            throw std::runtime_error{"zone format_version 不符：檔內=" + std::to_string(version) +
                                     " 預期=" + std::to_string(kSaveFormatVersion)};
        }
        if (persistence_flags != kReservedPersistenceFlags) {
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
                        tiles.owner, ever_realized);
                const auto count64 = static_cast<std::uint64_t>(tiles.width) * tiles.height;
                if (tiles.width == 0 || tiles.height == 0 ||
                    count64 > std::numeric_limits<std::size_t>::max() / 4U) {
                    throw std::runtime_error{"zone RegionTiles 尺寸超出可表達範圍"};
                }
                tiles.site.resize(static_cast<std::size_t>(count64));
                if (!tiles.valid_layout() || ever_realized.size() != tiles.tile_count()) {
                    throw std::runtime_error{"zone RegionTiles 欄位尺寸不一致"};
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
        case 2:
            payload = zone::SitePayload{};
            break;
        case 3:
            payload = zone::LocalPayload{};
            break;
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
        load_registry_snapshot(value->reg, registry_archive, AllComponents{});
    }
    if (stream.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error{"zone 檔含未解析的尾端資料"};
    }
    validate_zone_meta(*value);
    return value;
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
