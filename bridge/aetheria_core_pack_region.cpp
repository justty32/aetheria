#include "bridge/aetheria_core.h"

#include "core/api/version.h"
#include "core/runtime/save_raws.h"
#include "core/runtime/character_save.h"
#include "core/rules/ruleset.h"
#include "core/runtime/playable_session.h"
#include "core/zone/file_zone_store.h"
#include "core/time/tick.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/region_generator.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "bridge/aetheria_core_detail.h"

namespace aetheria::bridge::detail {

[[nodiscard]] godot::PackedInt32Array
pack_elevation(const std::vector<std::uint16_t> &source) {
  godot::PackedInt32Array packed;
  packed.resize(static_cast<std::int64_t>(source.size()));
  for (std::size_t index = 0; index < source.size(); ++index) {
    packed.set(static_cast<std::int64_t>(index), source[index]);
  }
  return packed;
}

[[nodiscard]] godot::PackedByteArray
pack_bytes(const std::vector<std::uint8_t> &source) {
  godot::PackedByteArray packed;
  packed.resize(static_cast<std::int64_t>(source.size()));
  for (std::size_t index = 0; index < source.size(); ++index) {
    packed.set(static_cast<std::int64_t>(index), source[index]);
  }
  return packed;
}

[[nodiscard]] godot::Dictionary
pack_portals(std::span<const world::RegionPortal> portals,
             std::span<const rules::WorldGraphConnection> connections) {
  godot::PackedInt32Array x;
  godot::PackedInt32Array y;
  godot::PackedInt32Array channels;
  godot::PackedInt32Array types;
  const auto size = static_cast<std::int64_t>(portals.size());
  x.resize(size);
  y.resize(size);
  channels.resize(size);
  types.resize(size);
  for (std::size_t index = 0; index < portals.size(); ++index) {
    const auto &portal = portals[index];
    const auto connection = std::ranges::find_if(
        connections, [&portal](const rules::WorldGraphConnection &candidate) {
          return candidate.id == portal.channel;
        });
    if (connection == connections.end()) {
      throw std::runtime_error{"Region portal 引用不存在的 WorldGraph 通道"};
    }
    const auto packed_index = static_cast<std::int64_t>(index);
    x.set(packed_index, portal.tile.x);
    y.set(packed_index, portal.tile.y);
    channels.set(packed_index,
                 static_cast<std::int32_t>(rules::value_of(portal.channel)));
    types.set(packed_index, static_cast<std::int32_t>(connection->type));
  }
  godot::Dictionary result;
  result["x"] = x;
  result["y"] = y;
  result["channel"] = channels;
  result["type"] = types;
  return result;
}

} // namespace aetheria::bridge::detail
