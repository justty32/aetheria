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


namespace aetheria::bridge {

namespace {

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
  godot::ClassDB::bind_method(
      godot::D_METHOD("new_game", "seed", "region_id", "data_path"),
      &AetheriaCore::new_game);
  godot::ClassDB::bind_method(
      godot::D_METHOD("save_game", "slot_path", "character_name"),
      &AetheriaCore::save_game);
  godot::ClassDB::bind_method(
      godot::D_METHOD("load_game", "slot_path", "character_name"),
      &AetheriaCore::load_game);
  godot::ClassDB::bind_method(
      godot::D_METHOD("list_saves", "saves_directory"),
      &AetheriaCore::list_saves);
  godot::ClassDB::bind_method(
      godot::D_METHOD("list_characters", "slot_path"),
      &AetheriaCore::list_characters);
  godot::ClassDB::bind_method(
      godot::D_METHOD("get_playable_snapshot"),
      &AetheriaCore::get_playable_snapshot);
  godot::ClassDB::bind_method(
      godot::D_METHOD("issue_move", "unit_id", "x", "y"),
      &AetheriaCore::issue_move);
  godot::ClassDB::bind_method(godot::D_METHOD("advance_xun"),
                              &AetheriaCore::advance_xun);
  godot::ClassDB::bind_method(
      godot::D_METHOD("resolve_encounter", "choice"),
      &AetheriaCore::resolve_encounter);
  godot::ClassDB::bind_method(
      godot::D_METHOD("coverage_command", "command"),
      &AetheriaCore::coverage_command);
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
