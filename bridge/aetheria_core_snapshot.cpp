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

namespace aetheria::bridge {

using namespace detail;

namespace {

[[nodiscard]] godot::PackedByteArray
pack_tile_blob(const world::RegionTiles &tiles) {
  constexpr std::size_t bytes_per_tile = 10;
  godot::PackedByteArray packed;
  packed.resize(static_cast<std::int64_t>(tiles.tile_count() * bytes_per_tile));
  const auto write_u16 = [&](std::size_t offset, std::uint16_t value) {
    packed.set(static_cast<std::int64_t>(offset),
               static_cast<std::uint8_t>(value & 0xffU));
    packed.set(static_cast<std::int64_t>(offset + 1U),
               static_cast<std::uint8_t>(value >> 8U));
  };
  for (std::size_t index = 0; index < tiles.tile_count(); ++index) {
    const auto offset = index * bytes_per_tile;
    write_u16(offset, rules::value_of(tiles.base[index]));
    write_u16(offset + 2U, rules::value_of(tiles.relief[index]));
    write_u16(offset + 4U, rules::value_of(tiles.feature[index]));
    write_u16(offset + 6U,
              static_cast<std::uint16_t>(tiles.owner[index]));
    packed.set(static_cast<std::int64_t>(offset + 8U),
               static_cast<std::uint8_t>(tiles.settlement[index]));
    packed.set(static_cast<std::int64_t>(offset + 9U), tiles.damage[index]);
  }
  return packed;
}

template <typename Row>
[[nodiscard]] godot::PackedInt32Array
pack_reduction(const world::RegionTiles &tiles) {
  const auto source = tiles.reduction_values<Row>();
  godot::PackedInt32Array packed;
  packed.resize(static_cast<std::int64_t>(source.size()));
  for (std::size_t index = 0; index < source.size(); ++index) {
    const auto value = std::min<std::uint64_t>(
        static_cast<std::uint64_t>(source[index]),
        static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()));
    packed.set(static_cast<std::int64_t>(index),
               static_cast<std::int32_t>(value));
  }
  return packed;
}

[[nodiscard]] godot::Dictionary
pack_grid(const std::optional<runtime::PlayableGridView> &view) {
  godot::Dictionary result;
  if (!view.has_value()) {
    return result;
  }
  result["width"] = static_cast<std::int64_t>(view->width);
  result["height"] = static_cast<std::int64_t>(view->height);
  result["z"] = static_cast<std::int64_t>(view->z);
  result["cells"] = pack_bytes(view->cells);
  result["player_x"] = view->player_x;
  result["player_y"] = view->player_y;
  return result;
}

} // namespace

godot::Dictionary AetheriaCore::get_playable_snapshot() const {
  if (!playable_) {
    return error_result("尚未開始新遊戲");
  }
  try {
    const auto started = std::chrono::steady_clock::now();
    const auto &tiles = playable_->tiles();
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
    result["population"] =
        pack_reduction<world::PopulationReduction>(tiles);
    result["order"] = pack_reduction<world::OrderReduction>(tiles);
    result["tile_blob"] = pack_tile_blob(tiles);
    // 顯示 palette 以 current data 的固定 id 序建立，與舊 generate_region 一致。
    const auto &display_rules = playable_->ruleset();
    result["terrain_ids"] = pack_definition_ids(display_rules.terrains());
    result["relief_ids"] = pack_definition_ids(display_rules.reliefs());
    result["feature_ids"] = pack_definition_ids(display_rules.features());
    result["edge_ids"] = pack_definition_ids(display_rules.edges());
    result["portals"] =
        pack_portals(tiles.portals, display_rules.world_connections());

    godot::Array units;
    for (const auto &unit : playable_->armies()) {
      godot::Dictionary packed;
      packed["id"] = static_cast<std::int64_t>(unit.id.uid);
      packed["faction"] = static_cast<std::int64_t>(
          static_cast<std::uint16_t>(unit.faction));
      packed["x"] = unit.tile.x;
      packed["y"] = unit.tile.y;
      packed["power"] = unit.power;
      packed["player"] = unit.player_controlled;
      units.push_back(packed);
    }
    result["units"] = units;

    godot::Array events;
    for (const auto &event : playable_->events()) {
      godot::Dictionary packed;
      packed["id"] = static_cast<std::int64_t>(event.id);
      packed["kind"] = static_cast<std::int64_t>(event.kind);
      packed["x"] = event.tile.x;
      packed["y"] = event.tile.y;
      packed["a"] = event.value_a;
      packed["b"] = event.value_b;
      events.push_back(packed);
    }
    result["events"] = events;
    result["encounter_pending"] = playable_->encounter_pending();
    result["revision"] = static_cast<std::int64_t>(playable_->revision());
    result["tick"] = static_cast<std::int64_t>(playable_->now());
    result["player_unit_id"] =
        static_cast<std::int64_t>(playable_->player_army_id().uid);
    result["guided_target_x"] = playable_->guided_target().x;
    result["guided_target_y"] = playable_->guided_target().y;
    result["battle_tile_x"] = playable_->battle_tile().x;
    result["battle_tile_y"] = playable_->battle_tile().y;
    if (playable_->battle_report()) {
      result["battle_report"] = pack_battle_report(*playable_->battle_report());
    } else {
      result["battle_report"] = godot::Dictionary{};
    }
    result["coverage"] = pack_coverage(playable_->coverage_summary());
    result["site_view"] = pack_grid(playable_->site_view());
    result["local_view"] = pack_grid(playable_->local_view());
    result["coverage_tile_x"] = playable_->coverage_tile().x;
    result["coverage_tile_y"] = playable_->coverage_tile().y;
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started);
    result["batch_bytes"] =
        static_cast<std::int64_t>(tiles.tile_count() * 10U);
    result["batch_ms"] = elapsed.count();
    return result;
  } catch (const std::exception &exception) {
    const godot::String message{exception.what()};
    godot::UtilityFunctions::push_error(message);
    return error_result(message);
  }
}

} // namespace aetheria::bridge
