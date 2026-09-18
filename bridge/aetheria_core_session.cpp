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

godot::Dictionary AetheriaCore::new_game(std::int64_t seed,
                                         std::int64_t region_id,
                                         const godot::String &data_path) {
  if (seed < 0 || region_id < 0 ||
      static_cast<std::uint64_t>(region_id) >
          std::numeric_limits<std::uint32_t>::max()) {
    return error_result(
        "seed 與 region_id 必須是非負整數，region_id 不得超過 uint32");
  }
  const std::filesystem::path data_directory{data_path.utf8().get_data()};
  if (!data_directory.is_absolute()) {
    return error_result("新世界 data 目錄必須使用絕對路徑");
  }
  try {
    auto ruleset = std::make_unique<rules::Ruleset>(
        rules::RulesetLoader::load(data_directory));
    auto store = std::make_unique<zone::InMemoryZoneStore>(*ruleset);
    auto playable = std::make_unique<runtime::PlayableSession>(
        static_cast<std::uint64_t>(seed),
        static_cast<std::uint32_t>(region_id), data_directory.string(),
        *store);
    playable_ruleset_ = std::move(ruleset);
    playable_store_ = std::move(store);
    playable_ = std::move(playable);
    godot::Dictionary result;
    result["ok"] = true;
    result["revision"] = static_cast<std::int64_t>(playable_->revision());
    return result;
  } catch (const std::exception &exception) {
    playable_.reset();
    playable_store_.reset();
    playable_ruleset_.reset();
    const godot::String message{exception.what()};
    godot::UtilityFunctions::push_error(message);
    return error_result(message);
  }
}

godot::Dictionary AetheriaCore::save_game(
    const godot::String &slot_path, const godot::String &character_name) {
  if (!playable_) {
    return error_result("尚未開始新遊戲");
  }
  const std::filesystem::path path{slot_path.utf8().get_data()};
  if (!path.is_absolute()) {
    return error_result("存檔槽必須使用絕對路徑");
  }
  try {
    zone::FileZoneStore destination{path, playable_->ruleset()};
    playable_->save_game(destination, character_name.utf8().get_data());
    godot::Dictionary result;
    result["ok"] = true;
    result["path"] = slot_path;
    return result;
  } catch (const std::exception &exception) {
    return error_result(godot::String::utf8(exception.what()));
  }
}

godot::Dictionary AetheriaCore::load_game(
    const godot::String &slot_path, const godot::String &character_name) {
  const std::filesystem::path path{slot_path.utf8().get_data()};
  if (!path.is_absolute()) {
    return error_result("讀檔槽必須使用絕對路徑");
  }
  try {
    const auto raws_directory = runtime::save_raws_directory(path);
    static_cast<void>(runtime::save_raws_hash(path));
    auto ruleset = std::make_unique<rules::Ruleset>(
        rules::RulesetLoader::load(raws_directory));
    auto store = std::make_unique<zone::FileZoneStore>(path, *ruleset);
    auto playable = runtime::PlayableSession::load(
        path, *store, character_name.utf8().get_data());
    playable_.reset();
    playable_store_.reset();
    playable_ruleset_.reset();
    playable_ruleset_ = std::move(ruleset);
    playable_store_ = std::move(store);
    playable_ = std::move(playable);
    godot::Dictionary result;
    result["ok"] = true;
    result["revision"] = static_cast<std::int64_t>(playable_->revision());
    return result;
  } catch (const std::exception &exception) {
    return error_result(godot::String::utf8(exception.what()));
  }
}

godot::PackedStringArray
AetheriaCore::list_saves(const godot::String &saves_directory) const {
  godot::PackedStringArray result;
  const std::filesystem::path root{saves_directory.utf8().get_data()};
  if (!root.is_absolute()) {
    return result;
  }
  std::error_code error;
  if (!std::filesystem::is_directory(root, error) || error) {
    return result;
  }
  std::vector<std::string> names;
  for (std::filesystem::directory_iterator iterator{root, error}, end;
       iterator != end && !error; iterator.increment(error)) {
    if (iterator->is_directory(error) && !error &&
        std::filesystem::is_regular_file(iterator->path() / "manifest.bin", error) &&
        !error) {
      names.push_back(iterator->path().filename().string());
    }
  }
  std::ranges::sort(names);
  for (const auto &name : names) {
    result.push_back(godot::String::utf8(name.c_str()));
  }
  return result;
}

godot::PackedStringArray
AetheriaCore::list_characters(const godot::String &slot_path) const {
  godot::PackedStringArray result;
  const std::filesystem::path slot{slot_path.utf8().get_data()};
  if (!slot.is_absolute()) {
    return result;
  }
  try {
    for (const auto &name : runtime::list_character_saves(slot)) {
      result.push_back(godot::String::utf8(name.c_str()));
    }
  } catch (const std::exception &exception) {
    godot::UtilityFunctions::push_error(godot::String::utf8(exception.what()));
  }
  return result;
}

} // namespace aetheria::bridge
