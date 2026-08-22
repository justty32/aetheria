#pragma once

// cross_zone.h：執行期跨 Local zone 的唯讀查詢、穩定引用與原子實體搬移。
// 只有 runtime consumer 可見；生成 target 刻意沒有本目錄的 include path。

#include "core/local/local_tiles.h"
#include "core/spatial/boundary_profile.h"
#include "core/zone/zone_key.h"

#include <cstdint>
#include <entt/entity/entity.hpp>
#include <optional>

namespace aetheria::zone {
class ZoneManager;
}

namespace aetheria::runtime {

// TileView 是 Local tile 的不可變值快照；不借用 zone 內部儲存。
// 回傳後不受 vector 重配或 zone 卸載影響。
struct TileView {
    rules::GroundId ground{};
    local::OverlayId overlay{local::OverlayId::None};
    local::EntityId occupant{};
    std::uint8_t light{};

    constexpr bool operator==(const TileView&) const noexcept = default;
};

// EdgeView 是 Local 有向邊的不可變值快照。
struct EdgeView {
    rules::EdgeId edge{};

    constexpr bool operator==(const EdgeView&) const noexcept = default;
};

// LocalPosition 是執行期實體所在 tile；屬易失層，不進 zone 存檔。
struct LocalPosition {
    local::LocalXY tile;

    constexpr bool operator==(const LocalPosition&) const noexcept = default;
};

// EntityRef 是唯一可跨 zone 保存的實體引用；解引用時重新查 uid_index。
struct EntityRef {
    zone::ZoneKey zone;
    std::uint64_t uid{};

    constexpr bool operator==(const EntityRef&) const noexcept = default;
};

// CrossZoneRuntime 是共同上層中介；只借用 ZoneManager，不擁有 zone 或實體。
// manager 析構後本物件失效；所有查詢只回值或 registry-local handle。
class CrossZoneRuntime {
public:
    explicit CrossZoneRuntime(zone::ZoneManager& manager) noexcept : manager_{manager} {}

    [[nodiscard]] std::optional<TileView> peek_tile(zone::ZoneKey key,
                                                    local::LocalXY tile) const noexcept;
    [[nodiscard]] std::optional<EdgeView> peek_edge(
        zone::ZoneKey key, local::LocalXY tile, spatial::BoundarySide direction) const noexcept;
    [[nodiscard]] std::optional<entt::entity> resolve(EntityRef reference) const noexcept;

    [[nodiscard]] bool migrate_entity(zone::ZoneKey from, entt::entity entity,
                                      zone::ZoneKey to, local::LocalXY at);

private:
    zone::ZoneManager& manager_;
};

}  // namespace aetheria::runtime
