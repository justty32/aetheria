#include "core/serialize/registry_codec.h"

#include <cereal/archives/portable_binary.hpp>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <entt/core/type_traits.hpp>
#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

namespace {

struct PositionProbe {
    std::int32_t x{};
    std::int32_t y{};

    template <typename Archive> void serialize(Archive& archive) { archive(x, y); }
};

struct HealthProbe {
    std::int32_t current{};
    std::int32_t maximum{};

    template <typename Archive> void serialize(Archive& archive) { archive(current, maximum); }
};

struct IdentityProbe {
    std::uint64_t stable_id{};

    template <typename Archive> void serialize(Archive& archive) { archive(stable_id); }
};

using PressureComponents = entt::type_list<PositionProbe, HealthProbe, IdentityProbe>;
static_assert(PressureComponents::size == 3);

[[nodiscard]] entt::registry make_pressure_registry() {
    entt::registry registry;
    std::vector<entt::entity> entities;
    entities.reserve(1'200);
    for (std::uint32_t index = 0; index < 1'200; ++index) {
        const auto entity = registry.create();
        registry.emplace<PositionProbe>(entity, static_cast<std::int32_t>(index),
                                        -static_cast<std::int32_t>(index));
        registry.emplace<HealthProbe>(entity, static_cast<std::int32_t>(index % 101U), 100);
        registry.emplace<IdentityProbe>(entity, UINT64_C(0xA000000000000000) + index);
        entities.push_back(entity);
    }
    for (std::size_t index = 0; index < entities.size(); index += 7) {
        registry.destroy(entities[index]);
        const auto replacement = registry.create();
        registry.emplace<PositionProbe>(replacement, static_cast<std::int32_t>(index), 77);
        registry.emplace<HealthProbe>(replacement, 88, 99);
        registry.emplace<IdentityProbe>(replacement, UINT64_C(0xB000000000000000) + index);
    }
    return registry;
}

[[nodiscard]] std::string encode_registry(const entt::registry& registry) {
    std::ostringstream stream{std::ios::binary};
    cereal::PortableBinaryOutputArchive cereal_archive{
        stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
    aetheria::serialize::RegistryOutputArchive archive{cereal_archive};
    aetheria::serialize::save_registry_snapshot(registry, archive, PressureComponents{});
    return std::move(stream).str();
}

[[nodiscard]] entt::registry decode_registry(std::string_view bytes) {
    std::istringstream stream{std::string{bytes}, std::ios::binary};
    cereal::PortableBinaryInputArchive cereal_archive{stream};
    aetheria::serialize::RegistryInputArchive archive{cereal_archive};
    entt::registry registry;
    aetheria::serialize::load_registry_snapshot(registry, archive, PressureComponents{});
    return registry;
}

TEST(RegistryCodec, DeterministicallyRoundTripsAtLeastOneThousandEntitiesAndThreeComponents) {
    const auto first_registry = make_pressure_registry();
    const auto first_bytes = encode_registry(first_registry);
    const auto second_bytes = encode_registry(make_pressure_registry());
    const auto loaded = decode_registry(first_bytes);
    const auto round_trip_bytes = encode_registry(loaded);

    EXPECT_EQ(first_registry.view<const PositionProbe>().size(), 1'200U);
    EXPECT_EQ(first_registry.view<const HealthProbe>().size(), 1'200U);
    EXPECT_EQ(first_registry.view<const IdentityProbe>().size(), 1'200U);
    EXPECT_EQ(loaded.view<const PositionProbe>().size(), 1'200U);
    EXPECT_EQ(loaded.view<const HealthProbe>().size(), 1'200U);
    EXPECT_EQ(loaded.view<const IdentityProbe>().size(), 1'200U);
    EXPECT_EQ(first_bytes, round_trip_bytes);
    EXPECT_EQ(first_bytes, second_bytes);
    RecordProperty("entity_count", 1'200);
    RecordProperty("component_type_count", PressureComponents::size);
    RecordProperty("canonical_bytes", first_bytes.size());
    std::cout << "RegistryCodec entities=1200 component_types=" << PressureComponents::size
              << " canonical_bytes=" << first_bytes.size() << '\n';
}

}  // namespace
