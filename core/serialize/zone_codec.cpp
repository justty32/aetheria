#include "core/serialize/zone_codec.h"

#include "core/serialize/all_components.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>

#include <entt/entity/snapshot.hpp>

#include <cstddef>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace aetheria::serialize {
namespace {

constexpr std::uint32_t kZoneMagic = UINT32_C(0x415A4F4E);
constexpr std::uint8_t kReservedPersistenceFlags = 0;
constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

using EntityValue = std::underlying_type_t<entt::entity>;

struct RegistryOutputArchive {
    cereal::PortableBinaryOutputArchive& archive;

    void operator()(entt::entity entity) { archive(static_cast<EntityValue>(entity)); }

    template <typename Value> void operator()(const Value& value) { archive(value); }
};

struct RegistryInputArchive {
    cereal::PortableBinaryInputArchive& archive;

    void operator()(entt::entity& entity) {
        EntityValue value{};
        archive(value);
        entity = static_cast<entt::entity>(value);
    }

    template <typename Value> void operator()(Value& value) { archive(value); }
};

template <typename... Components>
void save_registry(const entt::registry& registry, RegistryOutputArchive& archive,
                   entt::type_list<Components...>) {
    const auto snapshot = entt::snapshot{registry};
    snapshot.template get<entt::entity>(archive);
    (snapshot.template get<Components>(archive), ...);
}

template <typename... Components>
void load_registry(entt::registry& registry, RegistryInputArchive& archive,
                   entt::type_list<Components...>) {
    auto loader = entt::snapshot_loader{registry};
    loader.template get<entt::entity>(archive);
    (loader.template get<Components>(archive), ...);
    loader.orphans();
}

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

}  // namespace

std::string encode_zone(const zone::Zone& value) {
    validate_zone_meta(value);
    std::ostringstream stream{std::ios::binary};
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        const auto key = zone::value_of(value.key);
        const auto saved_tick = static_cast<std::int64_t>(value.last_saved_tick);
        const auto layer_count = static_cast<std::uint64_t>(value.layers.size());
        archive(kZoneMagic, kSaveFormatVersion, key, saved_tick, kReservedPersistenceFlags,
                layer_count);
        for (const auto& [z, grid] : value.layers) {
            archive(z, grid.width, grid.height, grid.tiles);
        }
    }
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        RegistryOutputArchive registry_archive{archive};
        save_registry(value.reg, registry_archive, AllComponents{});
    }
    if (!stream) {
        throw std::runtime_error{"zone 序列化失敗"};
    }
    return std::move(stream).str();
}

std::unique_ptr<zone::Zone> decode_zone(std::string_view bytes) {
    std::istringstream stream{std::string{bytes}, std::ios::binary};
    std::uint64_t key{};
    std::int64_t saved_tick{};
    std::uint64_t layer_count{};
    std::unique_ptr<zone::Zone> value;
    {
        cereal::PortableBinaryInputArchive archive{stream};
        std::uint32_t magic{};
        std::uint32_t version{};
        std::uint8_t persistence_flags{};
        archive(magic, version, key, saved_tick, persistence_flags, layer_count);
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
        if (layer_count > 256) {
            throw std::runtime_error{"zone layer 數超出 int8_t 可表達範圍"};
        }

        value = std::make_unique<zone::Zone>(zone::ZoneKey{key});
        value->last_saved_tick = time::Tick{saved_tick};
        value->layers.clear();
        for (std::uint64_t index = 0; index < layer_count; ++index) {
            std::int8_t z{};
            zone::TileGrid grid;
            archive(z, grid.width, grid.height, grid.tiles);
            const auto expected_size =
                static_cast<std::uint64_t>(grid.width) * static_cast<std::uint64_t>(grid.height);
            if (grid.width == 0 || grid.height == 0 ||
                expected_size > std::numeric_limits<std::size_t>::max() ||
                grid.tiles.size() != static_cast<std::size_t>(expected_size)) {
                throw std::runtime_error{"zone TileGrid 尺寸與 tile 數不符"};
            }
            if (!value->layers.emplace(z, std::move(grid)).second) {
                throw std::runtime_error{"zone layers 含重複 z"};
            }
        }
    }
    value->reg.clear();
    {
        cereal::PortableBinaryInputArchive registry_cereal{stream};
        RegistryInputArchive registry_archive{registry_cereal};
        load_registry(value->reg, registry_archive, AllComponents{});
    }
    if (stream.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error{"zone 檔含未解析的尾端資料"};
    }
    validate_zone_meta(*value);
    return value;
}

std::uint64_t persistent_state_hash(const zone::Zone& value) {
    const auto bytes = encode_zone(value);
    auto hash = kFnvOffset;
    for (const auto byte : bytes) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= kFnvPrime;
    }
    return hash;
}

}  // namespace aetheria::serialize
