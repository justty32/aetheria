#pragma once

// local_path_types.h：路徑位址、查詢與結果型別；由 local_path.h 引入。

#include "core/local/local_navigation.h"
#include "core/rules/ruleset.h"

#include <aetheria/runtime/cross_zone.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

namespace aetheria::local {

// z 是同一 Local zone 的 LocalPayload::layers key；水平位址沿用 LocalLocation。
struct LocalPathLocation {
  LocalLocation horizontal{};
  std::int8_t z{};

  constexpr auto
  operator<=>(const LocalPathLocation &) const noexcept = default;
};

// 門位址必須含 z，避免不同樓層同一 xy 邊共用門狀態。
struct LocalPathEdgeAddress {
  LocalPathLocation first{};
  LocalPathLocation second{};

  constexpr auto
  operator<=>(const LocalPathEdgeAddress &) const noexcept = default;
};

struct LocalPathStep {
  LocalPathLocation location{};
  LocalPathInteraction interaction{LocalPathInteraction::None};

  constexpr bool operator==(const LocalPathStep &) const noexcept = default;
};

struct LocalPathResult {
  LocalPathStatus status{LocalPathStatus::Unknown};
  std::vector<LocalPathStep> steps;
  // Coarse 時由上層 Site 規劃器回傳依序經過的 Local zone；精確格路徑留空。
  std::vector<zone::ZoneKey> coarse_zones;
  std::int64_t cost{};
  std::size_t expanded_tiles{};

  bool operator==(const LocalPathResult &) const = default;
};

using LocalPathTileQuery =
    std::function<std::optional<runtime::TileView>(LocalPathLocation)>;
using LocalPathEdgeQuery = std::function<std::optional<runtime::EdgeView>(
    LocalPathLocation, spatial::BoundarySide)>;
using LocalPathVerticalQuery =
    std::function<std::vector<std::int8_t>(LocalPathLocation)>;
using LocalPathDoorStateQuery =
    std::function<DoorState(const LocalPathEdgeAddress &)>;
using LocalCoarsePathQuery =
    std::function<std::optional<std::vector<zone::ZoneKey>>(zone::ZoneKey,
                                                            zone::ZoneKey)>;

// 查詢 callback 都是唯讀。vertical_neighbors 只可回同格的相鄰
// z；實作會排序去重。
struct LocalPathQueries {
  LocalPathTileQuery peek_tile;
  LocalPathEdgeQuery peek_edge;
  LocalPathVerticalQuery vertical_neighbors;
  LocalPathDoorStateQuery door_states;
  LocalCoarsePathQuery coarse_path;
  std::size_t max_expanded_tiles{100'000};
};

} // namespace aetheria::local
