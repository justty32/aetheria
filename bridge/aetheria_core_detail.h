#pragma once

#include "bridge/aetheria_core.h"

#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

#include <span>
#include <type_traits>
#include <vector>

namespace aetheria::bridge::detail {

[[nodiscard]] godot::PackedInt32Array
pack_elevation(const std::vector<std::uint16_t> &source);

[[nodiscard]] godot::PackedByteArray
pack_bytes(const std::vector<std::uint8_t> &source);

[[nodiscard]] godot::Dictionary
pack_portals(std::span<const world::RegionPortal> portals,
             std::span<const rules::WorldGraphConnection> connections);

[[nodiscard]] godot::Dictionary error_result(const godot::String &message);

[[nodiscard]] godot::Dictionary
pack_battle_report(const runtime::PlayableBattleReport &report);

[[nodiscard]] godot::Dictionary
pack_coverage(const runtime::PlayableCoverageSummary &summary);

template <typename Id>
[[nodiscard]] inline godot::PackedInt32Array pack_ids(const std::vector<Id> &source) {
  godot::PackedInt32Array packed;
  packed.resize(static_cast<std::int64_t>(source.size()));
  for (std::size_t index = 0; index < source.size(); ++index) {
    packed.set(static_cast<std::int64_t>(index),
               static_cast<std::int32_t>(
                   static_cast<std::underlying_type_t<Id>>(source[index])));
  }
  return packed;
}

template <typename Definition>
[[nodiscard]] inline godot::PackedStringArray
pack_definition_ids(std::span<const Definition> source) {
  godot::PackedStringArray packed;
  packed.resize(static_cast<std::int64_t>(source.size()));
  for (std::size_t index = 0; index < source.size(); ++index) {
    packed.set(static_cast<std::int64_t>(index),
               godot::String{source[index].id.c_str()});
  }
  return packed;
}

} // namespace aetheria::bridge::detail
