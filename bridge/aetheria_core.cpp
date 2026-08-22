#include "bridge/aetheria_core.h"

#include "core/api/version.h"
#include "core/rules/ruleset.h"
#include "core/time/tick.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/region_generator.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace aetheria::bridge {
namespace {

template <typename Id>
[[nodiscard]] godot::PackedInt32Array pack_ids(const std::vector<Id> &source) {
  godot::PackedInt32Array packed;
  packed.resize(static_cast<std::int64_t>(source.size()));
  for (std::size_t index = 0; index < source.size(); ++index) {
    packed.set(static_cast<std::int64_t>(index),
               static_cast<std::int32_t>(
                   static_cast<std::underlying_type_t<Id>>(source[index])));
  }
  return packed;
}

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

template <typename Definition>
[[nodiscard]] godot::PackedStringArray
pack_definition_ids(std::span<const Definition> source) {
  godot::PackedStringArray packed;
  packed.resize(static_cast<std::int64_t>(source.size()));
  for (std::size_t index = 0; index < source.size(); ++index) {
    packed.set(static_cast<std::int64_t>(index),
               godot::String{source[index].id.c_str()});
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

[[nodiscard]] godot::Dictionary error_result(const godot::String &message) {
  godot::Dictionary result;
  result["error"] = message;
  return result;
}

[[nodiscard]] godot::Dictionary
pack_localized_text(const narrative::LocalizedText &text) {
  godot::Dictionary parameters;
  for (const auto &argument : text.arguments) {
    godot::Dictionary packed_argument;
    packed_argument["value"] = godot::String{argument.value.c_str()};
    packed_argument["i18n"] =
        argument.kind == narrative::ArgumentKind::I18nKey;
    parameters[godot::String{argument.name.c_str()}] = packed_argument;
  }
  godot::Dictionary result;
  result["key"] = godot::String{text.key.c_str()};
  result["params"] = parameters;
  return result;
}

} // namespace

void AetheriaCore::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("get_core_version"),
                              &AetheriaCore::get_core_version);
  godot::ClassDB::bind_method(godot::D_METHOD("tick_to_date", "tick"),
                              &AetheriaCore::tick_to_date);
  godot::ClassDB::bind_method(
      godot::D_METHOD("generate_region", "seed", "region_id"),
      &AetheriaCore::generate_region);
  godot::ClassDB::bind_method(godot::D_METHOD("poll_events"),
                              &AetheriaCore::poll_events);
}

godot::String AetheriaCore::get_core_version() const {
  return godot::String{aetheria::core_version()};
}

godot::Dictionary AetheriaCore::tick_to_date(std::int64_t tick) const {
  const auto core_tick = aetheria::time::Tick{tick};
  if (!aetheria::time::is_representable(core_tick)) {
    return {};
  }
  const auto date = aetheria::time::to_date(core_tick);
  godot::Dictionary result;
  result["year"] = date.year;
  result["season"] = date.season;
  result["month"] = date.month;
  result["xun"] = date.xun;
  return result;
}

godot::Dictionary AetheriaCore::generate_region(std::int64_t seed,
                                                std::int64_t region_id) const {
  if (seed < 0 || region_id < 0 ||
      static_cast<std::uint64_t>(region_id) >
          std::numeric_limits<std::uint32_t>::max()) {
    return error_result(
        "seed 與 region_id 必須是非負整數，region_id 不得超過 uint32");
  }

  try {
    const auto ruleset = rules::RulesetLoader::load(AETHERIA_DEFAULT_DATA_DIR);
    const auto build = worldgen::build_skeleton(
        worldgen::RegionSlowVariables{static_cast<std::uint32_t>(region_id),
                                      128, 96},
        static_cast<std::uint64_t>(seed), ruleset);
    const auto tiles =
        worldgen::populate(build.skeleton, worldgen::RegionFastVariables{});

    godot::Dictionary result;
    result["width"] = static_cast<std::int64_t>(tiles.width);
    result["height"] = static_cast<std::int64_t>(tiles.height);
    result["base"] = pack_ids(tiles.base);
    result["relief"] = pack_ids(tiles.relief);
    result["feature"] = pack_ids(tiles.feature);
    result["temperature"] = pack_bytes(tiles.temperature);
    result["moisture"] = pack_bytes(tiles.moisture);
    result["elevation"] = pack_elevation(tiles.elevation);
    result["edges"] = pack_ids(tiles.edges);
    result["owner"] = pack_ids(tiles.owner);
    result["settlement"] = pack_ids(tiles.settlement);
    result["portals"] =
        pack_portals(tiles.portals, ruleset.world_connections());
    result["terrain_ids"] = pack_definition_ids(ruleset.terrains());
    result["relief_ids"] = pack_definition_ids(ruleset.reliefs());
    result["feature_ids"] = pack_definition_ids(ruleset.features());
    result["edge_ids"] = pack_definition_ids(ruleset.edges());
    return result;
  } catch (const std::exception &exception) {
    const godot::String message{exception.what()};
    godot::UtilityFunctions::push_error(message);
    return error_result(message);
  }
}

godot::Array AetheriaCore::poll_events() const {
  godot::Array result;
  for (const auto &event : event_feed_.poll()) {
    godot::Array lines;
    for (const auto &line : event.lines) {
      lines.push_back(pack_localized_text(line));
    }
    godot::Dictionary packed_event;
    packed_event["id"] = static_cast<std::int64_t>(event.id);
    packed_event["heading"] = pack_localized_text(event.heading);
    packed_event["lines"] = lines;
    result.push_back(packed_event);
  }
  return result;
}

} // namespace aetheria::bridge
