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

} // namespace aetheria::bridge
