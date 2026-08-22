#include "bridge/aetheria_core.h"

#include "core/api/version.h"
#include "core/rules/ruleset.h"
#include "core/runtime/playable_session.h"
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

[[nodiscard]] godot::Dictionary
pack_tile_state(const runtime::PlayableTileState &state) {
  godot::Dictionary result;
  result["owner"] = static_cast<std::int64_t>(
      static_cast<std::uint16_t>(state.owner));
  result["population"] = static_cast<std::int64_t>(state.population);
  result["order"] = static_cast<std::int64_t>(state.order);
  return result;
}

[[nodiscard]] godot::Dictionary
pack_battle_report(const runtime::PlayableBattleReport &report) {
  godot::Dictionary result;
  result["choice"] = report.choice == runtime::PlayableBattleChoice::CommandSite
                         ? "manual"
                         : "auto";
  result["tile_x"] = report.tile.x;
  result["tile_y"] = report.tile.y;
  result["loss_a"] = report.layer_result.loss_a;
  result["loss_b"] = report.layer_result.loss_b;
  result["region_expected_loss_a"] =
      report.layer_result.region_expected_loss_a;
  result["region_expected_loss_b"] =
      report.layer_result.region_expected_loss_b;
  result["outcome"] = static_cast<std::int64_t>(report.region_result.outcome);
  result["morale_delta_a"] = report.region_result.morale_delta_a;
  result["morale_delta_b"] = report.region_result.morale_delta_b;
  result["named_person"] = godot::String::utf8(report.named_person.c_str());
  result["named_outcome"] = static_cast<std::int64_t>(report.named_outcome);
  result["before"] = pack_tile_state(report.before);
  result["after"] = pack_tile_state(report.after);
  result["combat_result_source"] = "rules::CombatResult";
  result["layer_result_source"] =
      report.choice == runtime::PlayableBattleChoice::CommandSite
          ? "site::simulate_site_battle"
          : "world::resolve_scaled_combat(Region)";
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
      godot::D_METHOD("new_game", "seed", "region_id"),
      &AetheriaCore::new_game);
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

godot::Dictionary AetheriaCore::new_game(std::int64_t seed,
                                         std::int64_t region_id) {
  if (seed < 0 || region_id < 0 ||
      static_cast<std::uint64_t>(region_id) >
          std::numeric_limits<std::uint32_t>::max()) {
    return error_result(
        "seed 與 region_id 必須是非負整數，region_id 不得超過 uint32");
  }
  try {
    playable_ = std::make_unique<runtime::PlayableSession>(
        static_cast<std::uint64_t>(seed),
        static_cast<std::uint32_t>(region_id), AETHERIA_DEFAULT_DATA_DIR);
    godot::Dictionary result;
    result["ok"] = true;
    result["revision"] = static_cast<std::int64_t>(playable_->revision());
    return result;
  } catch (const std::exception &exception) {
    playable_.reset();
    const godot::String message{exception.what()};
    godot::UtilityFunctions::push_error(message);
    return error_result(message);
  }
}

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

godot::Dictionary AetheriaCore::issue_move(std::int64_t unit_id,
                                           std::int64_t x,
                                           std::int64_t y) {
  if (!playable_) {
    return error_result("尚未開始新遊戲");
  }
  if (unit_id <= 0) {
    return error_result("unit_id 必須是正整數");
  }
  const auto &tiles = playable_->tiles();
  if (x < 0 || y < 0 || x >= static_cast<std::int64_t>(tiles.width) ||
      y >= static_cast<std::int64_t>(tiles.height) ||
      x > std::numeric_limits<std::int16_t>::max() ||
      y > std::numeric_limits<std::int16_t>::max()) {
    return error_result("座標超出 Region 邊界");
  }
  try {
    playable_->issue_move(world::StableId{static_cast<std::uint64_t>(unit_id)},
                          {static_cast<std::int16_t>(x),
                           static_cast<std::int16_t>(y)});
    godot::Dictionary result;
    result["ok"] = true;
    result["revision"] = static_cast<std::int64_t>(playable_->revision());
    return result;
  } catch (const std::exception &exception) {
    return error_result(godot::String{exception.what()});
  }
}

godot::Dictionary AetheriaCore::advance_xun() {
  if (!playable_) {
    return error_result("尚未開始新遊戲");
  }
  try {
    const auto started = std::chrono::steady_clock::now();
    const auto report = playable_->advance_xun();
    godot::Dictionary result;
    result["ok"] = true;
    result["stage_count"] = static_cast<std::int64_t>(report.stages.size());
    result["ai_count"] = static_cast<std::int64_t>(report.ai_actions.size());
    result["encounter_pending"] = report.encounter_pending;
    result["elapsed_ms"] = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - started)
                                .count();
    return result;
  } catch (const std::exception &exception) {
    return error_result(godot::String{exception.what()});
  }
}

godot::Dictionary
AetheriaCore::resolve_encounter(const godot::String &choice) {
  if (!playable_) {
    return error_result("尚未開始新遊戲");
  }
  runtime::PlayableBattleChoice parsed;
  if (choice == "manual") {
    parsed = runtime::PlayableBattleChoice::CommandSite;
  } else if (choice == "auto") {
    parsed = runtime::PlayableBattleChoice::AutoRegion;
  } else {
    return error_result("戰鬥選擇只接受 manual 或 auto");
  }
  try {
    godot::Dictionary result;
    result["ok"] = true;
    result["report"] = pack_battle_report(playable_->resolve_encounter(parsed));
    return result;
  } catch (const std::exception &exception) {
    return error_result(godot::String{exception.what()});
  }
}

} // namespace aetheria::bridge
