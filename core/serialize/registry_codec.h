#pragma once

#include <cereal/archives/portable_binary.hpp>

#include <entt/core/type_info.hpp>
#include <entt/entity/registry.hpp>
#include <entt/entity/snapshot.hpp>

#include <type_traits>

namespace aetheria::serialize {

// EntityValue 是 registry 位元流使用的 EnTT entity 底層值型別。
// 它是無擁有者的值型別。
// 值本身永不失效；轉回 entity 後仍受所屬 registry 生命週期約束。
using EntityValue = std::underlying_type_t<entt::entity>;

// RegistryOutputArchive 是 EnTT snapshot 到 PortableBinary 的型別轉接器。
// 呼叫端擁有 cereal archive，它只借用。
// cereal archive 失效後本物件即不可使用。
struct RegistryOutputArchive {
    cereal::PortableBinaryOutputArchive& archive;

    void operator()(entt::entity entity) { archive(static_cast<EntityValue>(entity)); }

    template <typename Value> void operator()(const Value& value) { archive(value); }
};

// RegistryInputArchive 是 PortableBinary 到 EnTT snapshot_loader 的型別轉接器。
// 呼叫端擁有 cereal archive，它只借用。
// cereal archive 失效後本物件即不可使用。
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
void save_registry_snapshot(const entt::registry& registry, RegistryOutputArchive& archive,
                            entt::type_list<Components...>) {
    const auto snapshot = entt::snapshot{registry};
    snapshot.template get<entt::entity>(archive);
    (snapshot.template get<Components>(archive), ...);
}

template <typename... Components>
void load_registry_snapshot(entt::registry& registry, RegistryInputArchive& archive,
                            entt::type_list<Components...>) {
    auto loader = entt::snapshot_loader{registry};
    loader.template get<entt::entity>(archive);
    (loader.template get<Components>(archive), ...);
    loader.orphans();
}

}  // namespace aetheria::serialize
