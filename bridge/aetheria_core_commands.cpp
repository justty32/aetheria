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

godot::Dictionary
AetheriaCore::coverage_command(const godot::String &command) {
  if (!playable_) {
    return error_result("尚未開始新遊戲");
  }
  try {
    if (command == "enter_site_manual") {
      playable_->enter_site();
    } else if (command == "enter_site_auto") {
      playable_->manage_city();
    } else if (command == "leave_site") {
      playable_->leave_site();
    } else if (command == "build_city") {
      playable_->build_city();
    } else if (command == "accept_bandit") {
      playable_->accept_bandit_quest();
    } else if (command == "enter_local_manual") {
      playable_->enter_local();
    } else if (command == "enter_local_auto") {
      playable_->manage_local();
    } else if (command == "leave_local") {
      playable_->leave_local();
    } else if (command == "open_door") {
      playable_->open_door_and_move();
    } else if (command == "suppress_bandits") {
      playable_->suppress_bandits();
    } else if (command == "enter_dungeon_manual") {
      playable_->enter_dungeon();
    } else if (command == "enter_dungeon_auto") {
      playable_->manage_dungeon();
    } else if (command == "leave_dungeon") {
      playable_->leave_dungeon();
    } else if (command == "descend_dungeon") {
      playable_->descend_dungeon();
    } else if (command == "clear_dungeon") {
      playable_->clear_dungeon();
    } else if (command == "sign_treaty") {
      playable_->sign_peace_treaty();
    } else if (command == "measure_roundtrips") {
      playable_->measure_site_roundtrips();
    } else if (command == "measure_calibration") {
      playable_->measure_city_management(100);
    } else {
      return error_result("未知三層操作命令");
    }
    godot::Dictionary result;
    result["ok"] = true;
    result["revision"] = static_cast<std::int64_t>(playable_->revision());
    result["coverage"] = pack_coverage(playable_->coverage_summary());
    return result;
  } catch (const std::exception &exception) {
    return error_result(godot::String::utf8(exception.what()));
  }
}

} // namespace aetheria::bridge
