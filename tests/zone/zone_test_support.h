#pragma once

// tests/zone 底下多個測試檔共用的 fixture／helper：暫存目錄、填充過的 Zone、實體計數。

#include "core/time/tick.h"
#include "core/world/region_tiles.h"
#include "core/zone/lod_level.h"
#include "core/zone/zone.h"
#include "core/zone/zone_key.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

#include <entt/entity/registry.hpp>

namespace aetheria::tests {

constexpr auto kRegion = zone::child_key(zone::kRootZone, UINT16_C(0xA55A), 0);
constexpr auto kSite = zone::child_key(kRegion, UINT16_C(0x0ABC), UINT16_C(0x0123));
constexpr auto kLocal = zone::child_key(kSite, UINT16_C(0x0321), UINT16_C(0x0012));

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        static std::atomic_uint64_t sequence{};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("aetheria-zone-store-" + std::to_string(stamp) + "-" +
                 std::to_string(sequence.fetch_add(1)));
        if (!std::filesystem::create_directories(path_)) {
            throw std::runtime_error{"無法建立測試暫存目錄"};
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

[[nodiscard]] inline std::size_t entity_count(const zone::Zone& zone) {
    const auto* entities = zone.reg.storage<entt::entity>();
    return entities == nullptr ? 0 : entities->free_list();
}

[[nodiscard]] inline zone::Zone populated_zone(zone::ZoneKey key) {
    zone::Zone zone{key};
    zone.last_saved_tick = time::Tick{987'654'321};
    auto& layers = std::get<zone::RegionPayload>(zone.payload).layers;
    auto& surface = layers.emplace(0, world::RegionTiles{2, 2}).first->second;
    surface.temperature = {11, 22, 33, 44};
    surface.moisture = {55, 66, 77, 88};
    surface.settlement.at(1) = world::SettlementTier::Town;
    surface.portals.push_back(
        {{1, 1}, static_cast<rules::WorldConnectionId>(UINT32_C(37))});
    surface.site.at(0).lod = zone::LodLevel::Full;
    surface.site.at(0).ever_realized = true;
    auto& underground = layers.emplace(-1, world::RegionTiles{3, 1}).first->second;
    underground.elevation = {99, 100, 101};
    const auto extra = zone.reg.create();
    zone.reg.emplace<zone::ZoneMeta>(extra, zone::value_of(key));
    return zone;
}

}  // namespace aetheria::tests
