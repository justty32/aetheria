#pragma once

#include <cstdint>
#include <entt/entity/registry.hpp>
#include <map>
#include <stdexcept>
#include <utility>
#include <variant>

#include "core/local/local_tiles.h"
#include "core/site/site_projection.h"
#include "core/time/tick.h"
#include "core/world/region_tiles.h"
#include "core/zone/lod_level.h"
#include "core/zone/zone_key.h"

namespace aetheria::zone {

// ZoneMeta 是每個 zone registry 至少一個實體持有的存檔哨兵。
// 所屬 Zone 擁有 component 實例。
// 所屬 Zone 析構後失效；zone_key 必須與 Zone::key 相同。
struct ZoneMeta {
    std::uint64_t zone_key{};

    template <typename Archive> void serialize(Archive& archive) { archive(zone_key); }

    constexpr bool operator==(const ZoneMeta&) const noexcept = default;
};

// RegionPayload 是 L1 Zone 的垂直 RegionTiles 集合。
// 所屬 Zone 擁有它。
// payload alternative 被替換或 Zone 析構後失效。
struct RegionPayload {
    std::map<std::int8_t, world::RegionTiles> layers;
};

// SitePayload 是 L2 Zone 的三層資料；只有 persistent 由序列化白名單寫盤。
// 所屬 Zone 擁有它。
// payload alternative 被替換或 Zone 析構後失效。
struct SitePayload {
    site::SiteLayers layers;
};

// LocalPayload 是 L3 Zone 的 z→程序 tiles；本里程碑尚無持久 Local 欄位。
// 所屬 Zone 擁有它。
// payload alternative 被替換或 Zone 析構後失效。
struct LocalPayload {
    std::map<std::int8_t, local::LocalTiles> layers;
};

// SpatialPayload 將三層異質 tile schema 收在單一 Zone 欄位。
// Zone 擁有目前的 alternative。
// alternative 被替換或 Zone 析構後其中資料失效。
using SpatialPayload = std::variant<std::monostate, RegionPayload, SitePayload, LocalPayload>;

[[nodiscard]] inline bool payload_matches_level(ZoneKey key,
                                                const SpatialPayload& payload) noexcept {
    switch (level_of(key)) {
    case ZoneLevel::Root:
    case ZoneLevel::Detached:
        return std::holds_alternative<std::monostate>(payload);
    case ZoneLevel::Region:
        return std::holds_alternative<RegionPayload>(payload);
    case ZoneLevel::Site:
        return std::holds_alternative<SitePayload>(payload);
    case ZoneLevel::Local:
        return std::holds_alternative<LocalPayload>(payload);
    }
    return false;
}

[[nodiscard]] inline SpatialPayload default_payload_for(ZoneKey key) {
    switch (level_of(key)) {
    case ZoneLevel::Root:
    case ZoneLevel::Detached:
        return std::monostate{};
    case ZoneLevel::Region:
        return RegionPayload{};
    case ZoneLevel::Site:
        return SitePayload{};
    case ZoneLevel::Local:
        return LocalPayload{};
    }
    throw std::invalid_argument{"保留的 ZoneLevel 沒有 SpatialPayload schema"};
}

// Zone 是三層地圖共用的實體、垂直格網與生命週期狀態。
// ZoneManager 或 ZoneStore 以 unique_ptr 單獨擁有它。
// 被 unload／destroy 或其擁有者析構後失效。
struct Zone {
    explicit Zone(ZoneKey zone_key) : Zone{zone_key, default_payload_for(zone_key)} {}

    Zone(ZoneKey zone_key, SpatialPayload spatial_payload)
        : key{zone_key}, payload{std::move(spatial_payload)} {
        if (!payload_matches_level(key, payload)) {
            throw std::invalid_argument{"SpatialPayload alternative 與 ZoneKey level 不符"};
        }
        const auto placeholder = reg.create();
        reg.emplace<ZoneMeta>(placeholder, value_of(key));
    }

    Zone(const Zone&) = delete;
    Zone& operator=(const Zone&) = delete;
    Zone(Zone&&) noexcept = default;
    Zone& operator=(Zone&&) noexcept = default;

    ZoneKey key;
    entt::registry reg;
    SpatialPayload payload;
    LodLevel lod{LodLevel::Full};
    bool pinned{};
    time::Tick last_saved_tick{};
};

} // namespace aetheria::zone
