#pragma once

#include "core/time/tick.h"
#include "core/world/region_tiles.h"
#include "core/zone/lod_level.h"
#include "core/zone/zone_key.h"

#include <cstdint>
#include <optional>
#include <utility>

#include <entt/entity/registry.hpp>

namespace aetheria::zone {

// ZoneMeta 是每個 zone registry 至少一個實體持有的存檔哨兵。
// 所屬 Zone 擁有 component 實例。
// 所屬 Zone 析構後失效；zone_key 必須與 Zone::key 相同。
struct ZoneMeta {
    std::uint64_t zone_key{};

    template <typename Archive> void serialize(Archive& archive) { archive(zone_key); }

    constexpr bool operator==(const ZoneMeta&) const noexcept = default;
};

// Zone 是三層地圖共用的實體、垂直格網與生命週期狀態。
// ZoneManager 或 ZoneStore 以 unique_ptr 單獨擁有它。
// 被 unload／destroy 或其擁有者析構後失效。
struct Zone {
    explicit Zone(ZoneKey zone_key) : key{zone_key} {
        const auto placeholder = reg.create();
        reg.emplace<ZoneMeta>(placeholder, value_of(key));
    }

    Zone(const Zone&) = delete;
    Zone& operator=(const Zone&) = delete;
    Zone(Zone&&) noexcept = default;
    Zone& operator=(Zone&&) noexcept = default;

    ZoneKey key;
    entt::registry reg;
    // M1.0 只落地 L1 payload；三層異質 layers 的最終形狀待規劃者裁定。
    std::optional<world::RegionTiles> region_tiles;
    LodLevel lod{LodLevel::Full};
    bool pinned{};
    time::Tick last_saved_tick{};
};

}  // namespace aetheria::zone
