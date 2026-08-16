#pragma once

#include "core/time/tick.h"
#include "core/zone/zone_key.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include <entt/entity/registry.hpp>

namespace aetheria::zone {

// LodLevel 是 zone 當前載入與模擬的解析度。
// Zone 擁有這個執行期狀態。
// 所屬 Zone 析構後失效。
enum class LodLevel : std::uint8_t {
    Full,
    Coarse,
    Frozen,
    Absent,
};

// ZoneMeta 是每個 zone registry 至少一個實體持有的存檔哨兵。
// 所屬 Zone 擁有 component 實例。
// 所屬 Zone 析構後失效；zone_key 必須與 Zone::key 相同。
struct ZoneMeta {
    std::uint64_t zone_key{};

    template <typename Archive> void serialize(Archive& archive) { archive(zone_key); }

    constexpr bool operator==(const ZoneMeta&) const noexcept = default;
};

// TileGrid 是 M0.5 生命週期測試用的最小實質格網。
// 所屬 Zone 擁有它。
// 所屬 Zone 析構後失效；M1 會依 worldmap.md 換成真正 SoA。
struct TileGrid {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint16_t> tiles;

    TileGrid() = default;
    TileGrid(std::uint32_t grid_width, std::uint32_t grid_height)
        : width{grid_width}, height{grid_height},
          tiles(static_cast<std::size_t>(grid_width) * grid_height) {
        AETH_CHECK(width > 0 && height > 0);
    }
};

// Zone 是三層地圖共用的實體、垂直格網與生命週期狀態。
// ZoneManager 或 ZoneStore 以 unique_ptr 單獨擁有它。
// 被 unload／destroy 或其擁有者析構後失效。
struct Zone {
    explicit Zone(ZoneKey zone_key) : key{zone_key} {
        layers.emplace(0, TileGrid{1, 1});
        const auto placeholder = reg.create();
        reg.emplace<ZoneMeta>(placeholder, value_of(key));
    }

    Zone(const Zone&) = delete;
    Zone& operator=(const Zone&) = delete;
    Zone(Zone&&) noexcept = default;
    Zone& operator=(Zone&&) noexcept = default;

    ZoneKey key;
    entt::registry reg;
    std::map<std::int8_t, TileGrid> layers;
    LodLevel lod{LodLevel::Full};
    bool pinned{};
    time::Tick last_saved_tick{};
};

}  // namespace aetheria::zone
