#pragma once

// local_navigation.h：Local 執行期視野與移動共用的跨 zone
// 格位址、邊位址與門狀態。

#include "core/local/local_tiles.h"
#include "core/spatial/boundary_profile.h"
#include "core/zone/zone_key.h"

#include <compare>
#include <cstdint>
#include <functional>
#include <optional>

namespace aetheria::local {

// LocalLocation 是同一 Site 內一個 Local zone 的格位址。
struct LocalLocation {
  zone::ZoneKey zone{};
  LocalXY tile{};

  constexpr auto operator<=>(const LocalLocation &) const noexcept = default;
};

// LocalEdgeAddress 以排序後的兩端格規範化同一條邊，正反查詢得到相同 key。
struct LocalEdgeAddress {
  LocalLocation first{};
  LocalLocation second{};

  constexpr auto operator<=>(const LocalEdgeAddress &) const noexcept = default;
};

enum class DoorState : std::uint8_t {
  Closed,
  Open,
  Locked,
};

// 空 query 代表所有可開啟邊目前都關閉。
using DoorStateQuery = std::function<DoorState(const LocalEdgeAddress &)>;

[[nodiscard]] std::optional<LocalLocation>
offset_location(LocalLocation origin, std::int32_t dx,
                std::int32_t dy) noexcept;

[[nodiscard]] std::optional<LocalLocation>
adjacent_location(LocalLocation origin,
                  spatial::BoundarySide direction) noexcept;

[[nodiscard]] std::optional<LocalEdgeAddress>
local_edge_address(LocalLocation origin,
                   spatial::BoundarySide direction) noexcept;

[[nodiscard]] DoorState query_door_state(const DoorStateQuery &query,
                                         const LocalEdgeAddress &edge);

} // namespace aetheria::local
