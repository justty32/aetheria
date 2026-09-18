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

namespace {

[[nodiscard]] godot::Dictionary
pack_tile_state(const runtime::PlayableTileState &state) {
  godot::Dictionary result;
  result["owner"] = static_cast<std::int64_t>(
      static_cast<std::uint16_t>(state.owner));
  result["population"] = static_cast<std::int64_t>(state.population);
  result["order"] = static_cast<std::int64_t>(state.order);
  return result;
}

} // namespace

[[nodiscard]] godot::Dictionary error_result(const godot::String &message) {
  godot::Dictionary result;
  result["error"] = message;
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

[[nodiscard]] godot::Dictionary
pack_coverage(const runtime::PlayableCoverageSummary &summary) {
  godot::Dictionary result;
  result["residence"] = static_cast<std::int64_t>(summary.residence);
  result["development"] = summary.development;
  result["order"] = summary.order;
  result["production"] = static_cast<std::int64_t>(summary.production);
  result["city_buildings"] = summary.city_buildings;
  result["quest_count"] = summary.quest_count;
  result["bandit_quest_available"] = summary.bandit_quest_available;
  result["bandit_quest_accepted"] = summary.bandit_quest_accepted;
  result["dungeon_quest_available"] = summary.dungeon_quest_available;
  result["dungeon_cleared"] = summary.dungeon_cleared;
  result["dungeon_density_before"] = summary.dungeon_density_before;
  result["dungeon_density_after"] = summary.dungeon_density_after;
  result["treaty_count"] = summary.treaty_count;
  result["last_order_before"] = summary.last_order_before;
  result["last_order_after"] = summary.last_order_after;
  result["last_development_before"] = summary.last_development_before;
  result["last_development_after"] = summary.last_development_after;
  godot::PackedStringArray hashes;
  hashes.resize(static_cast<std::int64_t>(summary.roundtrip_hashes.size()));
  for (std::size_t index = 0; index < summary.roundtrip_hashes.size(); ++index) {
    hashes.set(static_cast<std::int64_t>(index),
               godot::String::num_uint64(summary.roundtrip_hashes[index]));
  }
  result["roundtrip_hashes"] = hashes;
  result["calibration_n"] = summary.calibration_n;
  result["manual_total"] = static_cast<std::int64_t>(summary.manual_total);
  result["managed_total"] = static_cast<std::int64_t>(summary.managed_total);
  result["signed_relative_error_percent"] =
      summary.signed_relative_error_percent;
  return result;
}

} // namespace aetheria::bridge::detail
